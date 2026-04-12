#include "TestHarness.h"

#include "../cad/CadEngine.h"
#include "../cad/battery/BatteryEntities.h"
#include "../cad/battery/PackLayoutGenerator.h"
#include "../cad/battery/PackLayoutMetrics.h"
#include "../cad/commands/BatteryCommands.h"
#include "../cad/commands/CommandStack.h"
#include "../cad/core/CadDocument.h"
#include "../cad/geometry/BatteryGeometryGenerator.h"
#include "../cad/io/JsonCadDocumentIO.h"
#include "../cad/io/MeshLoader.h"
#include "../cad/picking/HitTester.h"
#include "../cad/render/RenderPacket.h"
#include "../src/SimulationMappingBuilder.h"

#include <QJsonDocument>
#include <QFile>
#include <QDir>

#include <array>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using CellKey = std::pair<int, int>;
using BusbarKey = std::pair<int, cad::battery::BusbarRole>;

std::ostream& operator<<(std::ostream& stream, cad::core::EntityId id)
{
    stream << id.value;
    return stream;
}

struct ManualHierarchy
{
    cad::core::CadDocument document;
    cad::core::EntityId pack_id{};
    cad::core::EntityId module_id{};
    cad::core::EntityId group_id{};
    cad::core::EntityId cell_id{};
};

ManualHierarchy createManualHierarchy()
{
    ManualHierarchy hierarchy;

    cad::battery::BatteryPackEntity pack;
    pack.label = "Pack";
    pack.center = {10.0f, 0.0f, 0.0f};
    pack.size = {100.0f, 50.0f, 40.0f};
    hierarchy.pack_id = hierarchy.document.addPack(pack).id;

    cad::battery::ModuleBoundaryEntity module;
    module.label = "Module";
    module.parent_id = hierarchy.pack_id;
    module.module_index = 0;
    module.center = {5.0f, 0.0f, 0.0f};
    module.size = {80.0f, 40.0f, 30.0f};
    hierarchy.module_id = hierarchy.document.addModuleBoundary(module).id;

    cad::battery::CellGroupEntity group;
    group.label = "Group";
    group.parent_id = hierarchy.module_id;
    group.group_index = 0;
    group.series_index = 0;
    group.simulation_group_index = 0;
    group.center = {2.0f, 0.0f, 0.0f};
    group.size = {30.0f, 20.0f, 10.0f};
    group.cell_count = 1;
    hierarchy.group_id = hierarchy.document.addCellGroup(group).id;

    cad::battery::CellEntity cell;
    cell.label = "Cell";
    cell.parent_id = hierarchy.group_id;
    cell.series_index = 0;
    cell.parallel_index = 0;
    cell.simulation_group_index = 0;
    cell.position = {1.0f, 2.0f, 3.0f};
    hierarchy.cell_id = hierarchy.document.addCell(cell).id;

    return hierarchy;
}

bool containsId(const std::vector<cad::core::EntityId>& ids, cad::core::EntityId id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

bool containsPickableEntity(const std::vector<cad::render::ScreenPickable>& pickables, cad::core::EntityId id)
{
    return std::any_of(pickables.begin(), pickables.end(), [id](const cad::render::ScreenPickable& pickable) {
        return pickable.entity_id == id;
    });
}

const cad::render::ScreenPickable* findPickableEntity(
    const std::vector<cad::render::ScreenPickable>& pickables,
    cad::core::EntityId id)
{
    for (const auto& pickable : pickables) {
        if (pickable.entity_id == id) {
            return &pickable;
        }
    }
    return nullptr;
}

std::string entityIdString(cad::core::EntityId entity_id)
{
    return QString::number(static_cast<qulonglong>(entity_id.value)).toStdString();
}

cad::core::EntityId entityIdOf(const cad::battery::EntityRecord& record)
{
    return std::visit([](const auto& entity) { return entity.id; }, record);
}

cad::core::EntityId parentIdOf(const cad::battery::EntityRecord& record)
{
    return std::visit([](const auto& entity) { return entity.parent_id; }, record);
}

std::vector<cad::battery::EntityRecord> captureSubtreeSnapshot(
    const cad::core::CadDocument& document,
    cad::core::EntityId root_id)
{
    std::vector<cad::battery::EntityRecord> snapshot;
    for (cad::core::EntityId entity_id : document.subtreeIds(root_id)) {
        const auto entity = document.snapshotEntity(entity_id);
        CAD_EXPECT(entity.has_value());
        snapshot.push_back(*entity);
    }
    return snapshot;
}

void expectVec3Equals(const cad::math::Vec3& actual, const cad::math::Vec3& expected)
{
    CAD_EXPECT_EQ(actual.x, expected.x);
    CAD_EXPECT_EQ(actual.y, expected.y);
    CAD_EXPECT_EQ(actual.z, expected.z);
}

void expectFloatNear(float actual, float expected, float tolerance = 0.001f)
{
    if (std::fabs(actual - expected) > tolerance) {
        cad::tests::fail(
            __FILE__,
            __LINE__,
            "expected " + cad::tests::toString(actual) + " to be within "
                + cad::tests::toString(tolerance) + " of "
                + cad::tests::toString(expected));
    }
}

void expectVec3Near(const cad::math::Vec3& actual, const cad::math::Vec3& expected, float tolerance = 0.001f)
{
    expectFloatNear(actual.x, expected.x, tolerance);
    expectFloatNear(actual.y, expected.y, tolerance);
    expectFloatNear(actual.z, expected.z, tolerance);
}

template <typename T>
void expectBaseEntityEquals(const T& actual, const T& expected)
{
    CAD_EXPECT_EQ(actual.id, expected.id);
    CAD_EXPECT_EQ(actual.parent_id, expected.parent_id);
    CAD_EXPECT(actual.kind == expected.kind);
    CAD_EXPECT_EQ(actual.label, expected.label);
    CAD_EXPECT_EQ(actual.visible, expected.visible);
    CAD_EXPECT_EQ(actual.selectable, expected.selectable);
    CAD_EXPECT(actual.property_modes.position == expected.property_modes.position);
    CAD_EXPECT(actual.property_modes.geometry == expected.property_modes.geometry);
    CAD_EXPECT(actual.property_modes.label == expected.property_modes.label);
    CAD_EXPECT(actual.property_modes.visibility == expected.property_modes.visibility);
    CAD_EXPECT_EQ(actual.simulation_group_index, expected.simulation_group_index);
}

template <typename T>
void expectEntityEquals(const T& actual, const T& expected)
{
    expectBaseEntityEquals(actual, expected);

    if constexpr (std::is_same_v<T, cad::battery::BatteryPackEntity>) {
        expectVec3Equals(actual.center, expected.center);
        expectVec3Equals(actual.size, expected.size);
        CAD_EXPECT_EQ(actual.series_count, expected.series_count);
        CAD_EXPECT_EQ(actual.parallel_count, expected.parallel_count);
        CAD_EXPECT_EQ(actual.cell_radius, expected.cell_radius);
        CAD_EXPECT_EQ(actual.cell_height, expected.cell_height);
        CAD_EXPECT_EQ(actual.spacing_x, expected.spacing_x);
        CAD_EXPECT_EQ(actual.spacing_z, expected.spacing_z);
        CAD_EXPECT(actual.layout_type == expected.layout_type);
    } else if constexpr (std::is_same_v<T, cad::battery::ModuleBoundaryEntity>) {
        CAD_EXPECT_EQ(actual.module_index, expected.module_index);
        expectVec3Equals(actual.center, expected.center);
        expectVec3Equals(actual.size, expected.size);
        CAD_EXPECT_EQ(actual.series_span, expected.series_span);
        CAD_EXPECT_EQ(actual.parallel_span, expected.parallel_span);
    } else if constexpr (std::is_same_v<T, cad::battery::CellGroupEntity>) {
        CAD_EXPECT_EQ(actual.group_index, expected.group_index);
        CAD_EXPECT_EQ(actual.series_index, expected.series_index);
        expectVec3Equals(actual.center, expected.center);
        expectVec3Equals(actual.size, expected.size);
        CAD_EXPECT_EQ(actual.cell_count, expected.cell_count);
    } else if constexpr (std::is_same_v<T, cad::battery::CellEntity>) {
        expectVec3Equals(actual.position, expected.position);
        CAD_EXPECT(actual.form_factor == expected.form_factor);
        CAD_EXPECT_EQ(actual.radius, expected.radius);
        CAD_EXPECT_EQ(actual.height, expected.height);
        CAD_EXPECT_EQ(actual.width, expected.width);
        CAD_EXPECT_EQ(actual.depth, expected.depth);
        CAD_EXPECT_EQ(actual.series_index, expected.series_index);
        CAD_EXPECT_EQ(actual.parallel_index, expected.parallel_index);
        CAD_EXPECT_EQ(actual.cell_type, expected.cell_type);
    } else if constexpr (std::is_same_v<T, cad::battery::BusbarEntity>) {
        CAD_EXPECT_EQ(actual.module_index, expected.module_index);
        CAD_EXPECT(actual.role == expected.role);
        expectVec3Equals(actual.center, expected.center);
        expectVec3Equals(actual.size, expected.size);
    } else if constexpr (std::is_same_v<T, cad::battery::CoolingPlateEntity>) {
        CAD_EXPECT_EQ(actual.plate_index, expected.plate_index);
        expectVec3Equals(actual.center, expected.center);
        expectVec3Equals(actual.size, expected.size);
    } else if constexpr (std::is_same_v<T, cad::battery::PackEnclosureEntity>) {
        CAD_EXPECT_EQ(actual.enclosure_index, expected.enclosure_index);
        expectVec3Equals(actual.center, expected.center);
        expectVec3Equals(actual.size, expected.size);
        CAD_EXPECT_EQ(actual.wall_thickness, expected.wall_thickness);
    }
}

void expectEntityRecordMatches(
    const cad::battery::EntityRecord& actual_record,
    const cad::battery::EntityRecord& expected_record)
{
    CAD_EXPECT_EQ(actual_record.index(), expected_record.index());
    std::visit(
        [&actual_record](const auto& expected_entity) {
            using T = std::decay_t<decltype(expected_entity)>;
            const T* actual_entity = std::get_if<T>(&actual_record);
            CAD_EXPECT(actual_entity != nullptr);
            expectEntityEquals(*actual_entity, expected_entity);
        },
        expected_record);
}

void expectSubtreeMatchesSnapshot(
    const cad::core::CadDocument& document,
    const std::vector<cad::battery::EntityRecord>& snapshot)
{
    for (const auto& expected_record : snapshot) {
        const cad::core::EntityId entity_id = entityIdOf(expected_record);
        CAD_EXPECT(document.hasEntity(entity_id));

        const auto actual_record = document.snapshotEntity(entity_id);
        CAD_EXPECT(actual_record.has_value());
        expectEntityRecordMatches(*actual_record, expected_record);

        const cad::core::EntityId parent_id = parentIdOf(expected_record);
        if (parent_id.isValid()) {
            CAD_EXPECT(containsId(document.childIds(parent_id), entity_id));
        }
    }
}

void expectSubtreeRemoved(
    const cad::core::CadDocument& document,
    const std::vector<cad::battery::EntityRecord>& snapshot)
{
    for (const auto& record : snapshot) {
        CAD_EXPECT(!document.hasEntity(entityIdOf(record)));
    }
}

void expectLayoutConfigEquals(
    const cad::battery::PackLayoutConfig& actual,
    const cad::battery::PackLayoutConfig& expected)
{
    CAD_EXPECT_EQ(actual.preset_name, expected.preset_name);
    CAD_EXPECT_EQ(actual.cells_in_series, expected.cells_in_series);
    CAD_EXPECT_EQ(actual.cells_in_parallel, expected.cells_in_parallel);
    CAD_EXPECT_EQ(actual.module_count, expected.module_count);
    CAD_EXPECT(actual.cell_form_factor == expected.cell_form_factor);
    CAD_EXPECT_EQ(actual.cell_radius, expected.cell_radius);
    CAD_EXPECT_EQ(actual.cell_height, expected.cell_height);
    CAD_EXPECT_EQ(actual.cell_width, expected.cell_width);
    CAD_EXPECT_EQ(actual.cell_depth, expected.cell_depth);
    CAD_EXPECT_EQ(actual.top_cap_outer_diameter, expected.top_cap_outer_diameter);
    CAD_EXPECT_EQ(actual.top_cap_inner_diameter, expected.top_cap_inner_diameter);
    CAD_EXPECT_EQ(actual.top_cap_shoulder_height, expected.top_cap_shoulder_height);
    CAD_EXPECT_EQ(actual.positive_terminal_diameter, expected.positive_terminal_diameter);
    CAD_EXPECT_EQ(actual.positive_terminal_height, expected.positive_terminal_height);
    CAD_EXPECT_EQ(actual.insulating_ring_outer_diameter, expected.insulating_ring_outer_diameter);
    CAD_EXPECT_EQ(actual.insulating_ring_inner_diameter, expected.insulating_ring_inner_diameter);
    CAD_EXPECT_EQ(actual.insulating_ring_height, expected.insulating_ring_height);
    CAD_EXPECT_EQ(actual.bottom_cap_height, expected.bottom_cap_height);
    CAD_EXPECT_EQ(actual.x_spacing, expected.x_spacing);
    CAD_EXPECT_EQ(actual.z_spacing, expected.z_spacing);
    CAD_EXPECT_EQ(actual.module_gap_x, expected.module_gap_x);
    CAD_EXPECT_EQ(actual.busbar_thickness, expected.busbar_thickness);
    CAD_EXPECT_EQ(actual.busbar_width, expected.busbar_width);
    CAD_EXPECT_EQ(actual.busbar_terminal_clearance, expected.busbar_terminal_clearance);
    CAD_EXPECT_EQ(actual.busbar_support_offset, expected.busbar_support_offset);
    CAD_EXPECT_EQ(actual.busbar_overlap_width, expected.busbar_overlap_width);
    CAD_EXPECT_EQ(actual.busbar_tab_width, expected.busbar_tab_width);
    CAD_EXPECT_EQ(actual.busbar_tab_depth, expected.busbar_tab_depth);
    CAD_EXPECT_EQ(actual.cooling_channel_thickness, expected.cooling_channel_thickness);
    CAD_EXPECT_EQ(actual.cooling_channel_depth, expected.cooling_channel_depth);
    CAD_EXPECT_EQ(actual.cooling_plate_margin_x, expected.cooling_plate_margin_x);
    CAD_EXPECT_EQ(actual.cooling_plate_margin_z, expected.cooling_plate_margin_z);
    CAD_EXPECT_EQ(actual.cooling_plate_offset_below_tray, expected.cooling_plate_offset_below_tray);
    CAD_EXPECT_EQ(actual.enclosure_wall_thickness, expected.enclosure_wall_thickness);
    CAD_EXPECT_EQ(actual.enclosure_floor_thickness, expected.enclosure_floor_thickness);
    CAD_EXPECT_EQ(actual.enclosure_floor_offset, expected.enclosure_floor_offset);
    CAD_EXPECT_EQ(actual.enclosure_clearance_x, expected.enclosure_clearance_x);
    CAD_EXPECT_EQ(actual.enclosure_clearance_z, expected.enclosure_clearance_z);
    CAD_EXPECT_EQ(actual.module_tray_base_thickness, expected.module_tray_base_thickness);
    CAD_EXPECT_EQ(actual.module_tray_wall_thickness, expected.module_tray_wall_thickness);
    CAD_EXPECT_EQ(actual.module_tray_wall_height, expected.module_tray_wall_height);
    CAD_EXPECT_EQ(actual.cell_seating_offset, expected.cell_seating_offset);
    CAD_EXPECT_EQ(actual.support_rib_thickness, expected.support_rib_thickness);
    CAD_EXPECT_EQ(actual.support_rib_height, expected.support_rib_height);
    CAD_EXPECT_EQ(actual.module_tray_margin_x, expected.module_tray_margin_x);
    CAD_EXPECT_EQ(actual.module_tray_margin_z, expected.module_tray_margin_z);
}

cad::battery::PackLayoutConfig makeLayoutConfig(int module_count)
{
    cad::battery::PackLayoutConfig config;
    config.cells_in_series = 5;
    config.cells_in_parallel = 2;
    config.module_count = module_count;
    return config;
}

cad::battery::PackLayoutConfig makeSingleCellLayoutConfig(cad::battery::CellFormFactor form_factor)
{
    cad::battery::PackLayoutConfig config;
    config.cells_in_series = 1;
    config.cells_in_parallel = 1;
    config.module_count = 1;
    config.cell_form_factor = form_factor;
    return config;
}

cad::core::CadDocument buildGeneratedDocument(const cad::battery::PackLayoutConfig& config)
{
    cad::core::CadDocument document;
    cad::battery::PackLayoutGenerator::rebuildDocument(document, config);
    return document;
}

std::optional<cad::battery::BoundingBox> geometryBoundsForLayer(
    const cad::geometry::GeometryBuffer& geometry,
    cad::geometry::SurfaceLayer layer)
{
    cad::math::Vec3 min_point{};
    cad::math::Vec3 max_point{};
    bool initialized = false;
    for (const auto& triangle : geometry.triangles) {
        if (triangle.layer != layer) {
            continue;
        }

        const cad::math::Vec3 vertices[] = {triangle.a, triangle.b, triangle.c};
        for (const cad::math::Vec3& vertex : vertices) {
            if (!initialized) {
                min_point = vertex;
                max_point = vertex;
                initialized = true;
                continue;
            }
            min_point.x = std::min(min_point.x, vertex.x);
            min_point.y = std::min(min_point.y, vertex.y);
            min_point.z = std::min(min_point.z, vertex.z);
            max_point.x = std::max(max_point.x, vertex.x);
            max_point.y = std::max(max_point.y, vertex.y);
            max_point.z = std::max(max_point.z, vertex.z);
        }
    }

    if (!initialized) {
        return std::nullopt;
    }

    return cad::battery::BoundingBox{
        {
            (min_point.x + max_point.x) * 0.5f,
            (min_point.y + max_point.y) * 0.5f,
            (min_point.z + max_point.z) * 0.5f
        },
        {
            max_point.x - min_point.x,
            max_point.y - min_point.y,
            max_point.z - min_point.z
        }
    };
}

float largestInteriorAbsXForLayer(
    const cad::geometry::GeometryBuffer& geometry,
    cad::geometry::SurfaceLayer layer,
    float center_x,
    float outer_half_width)
{
    float best = 0.0f;
    for (const auto& triangle : geometry.triangles) {
        if (triangle.layer != layer) {
            continue;
        }

        const cad::math::Vec3 vertices[] = {triangle.a, triangle.b, triangle.c};
        for (const cad::math::Vec3& vertex : vertices) {
            const float abs_x = std::fabs(vertex.x - center_x);
            if (abs_x < outer_half_width - 0.001f) {
                best = std::max(best, abs_x);
            }
        }
    }
    return best;
}

std::array<cad::math::Vec3, 8> boxCorners(const cad::math::Vec3& min_point, const cad::math::Vec3& max_point)
{
    return {{
        {min_point.x, min_point.y, min_point.z},
        {min_point.x, min_point.y, max_point.z},
        {min_point.x, max_point.y, min_point.z},
        {min_point.x, max_point.y, max_point.z},
        {max_point.x, min_point.y, min_point.z},
        {max_point.x, min_point.y, max_point.z},
        {max_point.x, max_point.y, min_point.z},
        {max_point.x, max_point.y, max_point.z}
    }};
}

constexpr std::array<std::array<int, 3>, 12> kBoxTriangles{{
    {{0, 4, 6}}, {{0, 6, 2}},
    {{5, 1, 3}}, {{5, 3, 7}},
    {{1, 0, 2}}, {{1, 2, 3}},
    {{4, 5, 7}}, {{4, 7, 6}},
    {{2, 6, 7}}, {{2, 7, 3}},
    {{1, 5, 4}}, {{1, 4, 0}}
}};

void writeBinaryBoxStl(const QString& path, const cad::math::Vec3& min_point, const cad::math::Vec3& max_point)
{
    QFile output(path);
    CAD_EXPECT(output.open(QIODevice::WriteOnly | QIODevice::Truncate));

    const std::array<cad::math::Vec3, 8> corners = boxCorners(min_point, max_point);
    const std::array<char, 80> header{};
    const std::uint32_t triangle_count = static_cast<std::uint32_t>(kBoxTriangles.size());
    CAD_EXPECT_EQ(output.write(header.data(), static_cast<qint64>(header.size())), static_cast<qint64>(header.size()));
    CAD_EXPECT_EQ(output.write(reinterpret_cast<const char*>(&triangle_count), static_cast<qint64>(sizeof(triangle_count))), static_cast<qint64>(sizeof(triangle_count)));

    for (const auto& triangle : kBoxTriangles) {
        const float normal[3] = {0.0f, 0.0f, 0.0f};
        float points[9];
        for (int vertex_index = 0; vertex_index < 3; ++vertex_index) {
            const cad::math::Vec3& point = corners[static_cast<std::size_t>(triangle[vertex_index])];
            points[vertex_index * 3] = point.x;
            points[vertex_index * 3 + 1] = point.y;
            points[vertex_index * 3 + 2] = point.z;
        }
        const std::uint16_t attribute = 0;
        CAD_EXPECT_EQ(output.write(reinterpret_cast<const char*>(normal), static_cast<qint64>(sizeof(normal))), static_cast<qint64>(sizeof(normal)));
        CAD_EXPECT_EQ(output.write(reinterpret_cast<const char*>(points), static_cast<qint64>(sizeof(points))), static_cast<qint64>(sizeof(points)));
        CAD_EXPECT_EQ(output.write(reinterpret_cast<const char*>(&attribute), static_cast<qint64>(sizeof(attribute))), static_cast<qint64>(sizeof(attribute)));
    }

    output.close();
    CAD_EXPECT(QFile::exists(path));
}

void writeAsciiBoxStl(const QString& path, const cad::math::Vec3& min_point, const cad::math::Vec3& max_point)
{
    QFile output(path);
    CAD_EXPECT(output.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));

    const std::array<cad::math::Vec3, 8> corners = boxCorners(min_point, max_point);
    QByteArray ascii_stl;
    ascii_stl.append("solid codex_box\n");
    for (const auto& triangle : kBoxTriangles) {
        ascii_stl.append("  facet normal 0 0 0\n");
        ascii_stl.append("    outer loop\n");
        for (int vertex_index = 0; vertex_index < 3; ++vertex_index) {
            const cad::math::Vec3& point = corners[static_cast<std::size_t>(triangle[vertex_index])];
            ascii_stl.append(
                QString("      vertex %1 %2 %3\n")
                    .arg(point.x, 0, 'f', 6)
                    .arg(point.y, 0, 'f', 6)
                    .arg(point.z, 0, 'f', 6)
                    .toUtf8());
        }
        ascii_stl.append("    endloop\n");
        ascii_stl.append("  endfacet\n");
    }
    ascii_stl.append("endsolid codex_box\n");

    CAD_EXPECT_EQ(output.write(ascii_stl), static_cast<qint64>(ascii_stl.size()));
    output.close();
    CAD_EXPECT(QFile::exists(path));
}

QString regressionArtifactPath(const QString& file_name)
{
    const QString artifact_dir = QDir::current().filePath("build-desktop-test-artifacts");
    CAD_EXPECT(QDir().mkpath(artifact_dir));

    const QString path = QDir(artifact_dir).filePath(file_name);
    QFile::remove(path);
    return path;
}

const cad::battery::ModuleBoundaryEntity* findModuleByIndex(const cad::core::CadDocument& document, int module_index)
{
    for (const auto& module : document.moduleBoundaries()) {
        if (module.module_index == module_index) {
            return &module;
        }
    }
    return nullptr;
}

const cad::battery::CellGroupEntity* findGroupBySeriesIndex(const cad::core::CadDocument& document, int series_index)
{
    for (const auto& group : document.cellGroups()) {
        if (group.series_index == series_index) {
            return &group;
        }
    }
    return nullptr;
}

std::vector<const cad::battery::CellGroupEntity*> orderedGroupsBySeries(const cad::core::CadDocument& document)
{
    std::vector<const cad::battery::CellGroupEntity*> groups;
    groups.reserve(document.cellGroups().size());
    for (const auto& group : document.cellGroups()) {
        groups.push_back(&group);
    }
    std::sort(groups.begin(), groups.end(), [](const auto* lhs, const auto* rhs) {
        if (lhs->series_index != rhs->series_index) {
            return lhs->series_index < rhs->series_index;
        }
        return lhs->id.value < rhs->id.value;
    });
    return groups;
}

const cad::battery::CellEntity* findCellByKey(const cad::core::CadDocument& document, int series_index, int parallel_index)
{
    for (const auto& cell : document.cells()) {
        if (cell.series_index == series_index && cell.parallel_index == parallel_index) {
            return &cell;
        }
    }
    return nullptr;
}

const cad::battery::BusbarEntity* findBusbarByKey(
    const cad::core::CadDocument& document,
    int module_index,
    cad::battery::BusbarRole role)
{
    for (const auto& busbar : document.busbars()) {
        if (busbar.module_index == module_index && busbar.role == role) {
            return &busbar;
        }
    }
    return nullptr;
}

const cad::battery::CoolingPlateEntity* findCoolingPlateByIndex(const cad::core::CadDocument& document, int plate_index)
{
    for (const auto& plate : document.coolingPlates()) {
        if (plate.plate_index == plate_index) {
            return &plate;
        }
    }
    return nullptr;
}

const cad::battery::PackEnclosureEntity* findEnclosureByIndex(const cad::core::CadDocument& document, int enclosure_index)
{
    for (const auto& enclosure : document.packEnclosures()) {
        if (enclosure.enclosure_index == enclosure_index) {
            return &enclosure;
        }
    }
    return nullptr;
}

int moduleIndexForSeries(const cad::battery::ResolvedPackLayout& resolved, int series_index)
{
    for (const auto& slice : resolved.modules) {
        if (series_index >= slice.series_start && series_index < slice.series_start + slice.series_count) {
            return slice.module_index;
        }
    }
    return -1;
}

void corruptParent(cad::core::CadDocument& document, cad::core::EntityId entity_id, cad::core::EntityId parent_id)
{
    auto snapshot = document.snapshotEntity(entity_id);
    CAD_EXPECT(snapshot.has_value());

    cad::battery::EntityRecord entity = *snapshot;
    std::visit([parent_id](auto& value) { value.parent_id = parent_id; }, entity);
    CAD_EXPECT(document.restoreEntity(entity));
}

void expectHierarchyMatchesLayout(const cad::core::CadDocument& document, const cad::battery::PackLayoutConfig& config)
{
    const cad::battery::ResolvedPackLayout resolved = cad::battery::resolvePackLayout(config);

    CAD_EXPECT_EQ(document.packs().size(), std::size_t{1});
    CAD_EXPECT_EQ(document.moduleBoundaries().size(), static_cast<std::size_t>(resolved.module_count));
    CAD_EXPECT_EQ(document.cellGroups().size(), static_cast<std::size_t>(resolved.series_count));
    CAD_EXPECT_EQ(document.cells().size(), static_cast<std::size_t>(resolved.series_count * resolved.parallel_count));
    CAD_EXPECT_EQ(document.busbars().size(), static_cast<std::size_t>(resolved.module_count * 2));
    CAD_EXPECT_EQ(document.coolingPlates().size(), static_cast<std::size_t>(resolved.module_count));
    CAD_EXPECT_EQ(document.packEnclosures().size(), std::size_t{1});

    const cad::battery::BatteryPackEntity& pack = document.packs().front();
    CAD_EXPECT(!pack.parent_id.isValid());

    for (const auto& module : document.moduleBoundaries()) {
        CAD_EXPECT_EQ(module.parent_id, pack.id);
    }

    for (const auto& group : document.cellGroups()) {
        const int expected_module_index = moduleIndexForSeries(resolved, group.series_index);
        CAD_EXPECT(expected_module_index >= 0);
        const auto* module = findModuleByIndex(document, expected_module_index);
        CAD_EXPECT(module != nullptr);
        CAD_EXPECT_EQ(group.parent_id, module->id);
    }

    for (const auto& cell : document.cells()) {
        const auto* group = findGroupBySeriesIndex(document, cell.series_index);
        CAD_EXPECT(group != nullptr);
        CAD_EXPECT_EQ(cell.parent_id, group->id);
    }

    for (const auto& busbar : document.busbars()) {
        const auto* module = findModuleByIndex(document, busbar.module_index);
        CAD_EXPECT(module != nullptr);
        CAD_EXPECT_EQ(busbar.parent_id, module->id);
    }

    for (const auto& plate : document.coolingPlates()) {
        const auto* module = findModuleByIndex(document, plate.plate_index);
        CAD_EXPECT(module != nullptr);
        CAD_EXPECT_EQ(plate.parent_id, module->id);
    }

    for (const auto& enclosure : document.packEnclosures()) {
        CAD_EXPECT_EQ(enclosure.parent_id, pack.id);
    }
}

} // namespace

CAD_TEST(cad_document_preserves_ids_and_parent_links_on_restore)
{
    ManualHierarchy hierarchy = createManualHierarchy();

    CAD_EXPECT_EQ(hierarchy.document.packs().size(), std::size_t{1});
    CAD_EXPECT_EQ(hierarchy.document.modules().size(), std::size_t{1});
    CAD_EXPECT_EQ(hierarchy.document.cellGroups().size(), std::size_t{1});
    CAD_EXPECT_EQ(hierarchy.document.cells().size(), std::size_t{1});

    CAD_EXPECT_EQ(hierarchy.document.findModuleBoundary(hierarchy.module_id)->parent_id, hierarchy.pack_id);
    CAD_EXPECT_EQ(hierarchy.document.findCellGroup(hierarchy.group_id)->parent_id, hierarchy.module_id);
    CAD_EXPECT_EQ(hierarchy.document.findCell(hierarchy.cell_id)->parent_id, hierarchy.group_id);
    CAD_EXPECT_EQ(hierarchy.document.worldPosition(hierarchy.cell_id).x, 18.0f);

    const auto group_snapshot = hierarchy.document.snapshotEntity(hierarchy.group_id);
    const auto cell_snapshot = hierarchy.document.snapshotEntity(hierarchy.cell_id);
    CAD_EXPECT(group_snapshot.has_value());
    CAD_EXPECT(cell_snapshot.has_value());

    CAD_EXPECT(hierarchy.document.removeEntity(hierarchy.group_id));
    CAD_EXPECT(!hierarchy.document.hasEntity(hierarchy.group_id));
    CAD_EXPECT(!hierarchy.document.hasEntity(hierarchy.cell_id));

    CAD_EXPECT(hierarchy.document.restoreEntity(*group_snapshot));
    CAD_EXPECT(hierarchy.document.restoreEntity(*cell_snapshot));

    const auto* restored_group = hierarchy.document.findCellGroup(hierarchy.group_id);
    const auto* restored_cell = hierarchy.document.findCell(hierarchy.cell_id);
    CAD_EXPECT(restored_group != nullptr);
    CAD_EXPECT(restored_cell != nullptr);
    CAD_EXPECT_EQ(restored_group->id, hierarchy.group_id);
    CAD_EXPECT_EQ(restored_group->parent_id, hierarchy.module_id);
    CAD_EXPECT_EQ(restored_cell->id, hierarchy.cell_id);
    CAD_EXPECT_EQ(restored_cell->parent_id, hierarchy.group_id);

    const std::vector<cad::core::EntityId> module_children = hierarchy.document.childIds(hierarchy.module_id);
    const std::vector<cad::core::EntityId> group_children = hierarchy.document.childIds(hierarchy.group_id);
    CAD_EXPECT(containsId(module_children, hierarchy.group_id));
    CAD_EXPECT(containsId(group_children, hierarchy.cell_id));
    CAD_EXPECT_EQ(hierarchy.document.subtreeIds(hierarchy.pack_id).size(), std::size_t{4});
}

CAD_TEST(json_cad_document_round_trip_preserves_structure_and_overrides)
{
    cad::core::CadDocument document;
    cad::battery::PackLayoutConfig config = makeLayoutConfig(2);
    config.preset_name = "Regression Layout";
    config.cell_form_factor = cad::battery::CellFormFactor::Cylindrical;
    config.cell_radius = 11.25f;
    config.cell_height = 72.5f;
    config.x_spacing = 24.5f;
    config.z_spacing = 25.5f;
    config.module_gap_x = 31.0f;
    config.cooling_plate_margin_x = 8.0f;
    config.enclosure_wall_thickness = 4.0f;
    config.module_tray_margin_z = 12.0f;

    cad::battery::PackLayoutGenerator::rebuildDocument(document, config);
    document.metadata().cell_mesh_path = "fixtures/cells/sample_cell.stl";

    const auto* pack = &document.packs().front();
    const auto* module1 = findModuleByIndex(document, 1);
    const auto* group4 = findGroupBySeriesIndex(document, 4);
    const auto* cell41 = findCellByKey(document, 4, 1);
    const auto* busbar1 = findBusbarByKey(document, 1, cad::battery::BusbarRole::Positive);
    const auto* plate1 = findCoolingPlateByIndex(document, 1);
    const auto* enclosure0 = findEnclosureByIndex(document, 0);

    CAD_EXPECT(module1 != nullptr);
    CAD_EXPECT(group4 != nullptr);
    CAD_EXPECT(cell41 != nullptr);
    CAD_EXPECT(busbar1 != nullptr);
    CAD_EXPECT(plate1 != nullptr);
    CAD_EXPECT(enclosure0 != nullptr);

    const cad::core::EntityId pack_id = pack->id;
    const cad::core::EntityId module1_id = module1->id;
    const cad::core::EntityId group4_id = group4->id;
    const cad::core::EntityId cell41_id = cell41->id;
    const cad::core::EntityId busbar1_id = busbar1->id;
    const cad::core::EntityId plate1_id = plate1->id;
    const cad::core::EntityId enclosure0_id = enclosure0->id;

    CAD_EXPECT(document.setEntityLabel(pack_id, "Pack override"));
    CAD_EXPECT(document.setEntityPosition(pack_id, {1.0f, 2.0f, 3.0f}));
    CAD_EXPECT(document.setEntityLabel(module1_id, "Module 2 override"));
    CAD_EXPECT(document.setModuleBoundaryGeometry(module1_id, {4.0f, 5.0f, 6.0f}, {70.0f, 80.0f, 90.0f}));
    CAD_EXPECT(document.setEntityLabel(group4_id, "Series 5 override"));
    CAD_EXPECT(document.setEntityPosition(group4_id, {7.0f, 8.0f, 9.0f}));
    CAD_EXPECT(document.setEntityVisibility(group4_id, false));
    CAD_EXPECT(document.setCellPosition(cell41_id, {10.0f, 11.0f, 12.0f}));
    CAD_EXPECT(document.setCellGeometry(cell41_id, 13.5f, 74.5f));
    CAD_EXPECT(document.setBusbarGeometry(busbar1_id, {14.0f, 15.0f, 16.0f}, {17.0f, 18.0f, 19.0f}));
    CAD_EXPECT(document.setCoolingPlateGeometry(plate1_id, {20.0f, 21.0f, 22.0f}, {23.0f, 24.0f, 25.0f}));
    CAD_EXPECT(document.setEnclosureGeometry(enclosure0_id, {26.0f, 27.0f, 28.0f}, {29.0f, 30.0f, 31.0f}, 4.5f));
    CAD_EXPECT(document.setEntityVisibility(enclosure0_id, false));
    document.selectEntity(cell41_id);

    const std::vector<cad::battery::EntityRecord> snapshot = captureSubtreeSnapshot(document, pack_id);
    const std::uint64_t next_entity_id = document.nextEntityIdValue();

    QJsonObject project_root;
    project_root.insert("cad_document", cad::io::serializeCadDocument(document));
    const QJsonDocument json_document(project_root);
    const QJsonDocument reparsed = QJsonDocument::fromJson(json_document.toJson(QJsonDocument::Indented));
    CAD_EXPECT(reparsed.isObject());

    cad::core::CadDocument loaded_document;
    QString load_error;
    CAD_EXPECT(cad::io::tryLoadCadDocumentFromProject(reparsed.object(), loaded_document, &load_error));
    CAD_EXPECT(load_error.isEmpty());

    expectLayoutConfigEquals(loaded_document.metadata().layout_config, config);
    CAD_EXPECT_EQ(loaded_document.metadata().cell_mesh_path, std::string("fixtures/cells/sample_cell.stl"));
    CAD_EXPECT_EQ(loaded_document.selection().primary, cell41_id);
    CAD_EXPECT_EQ(loaded_document.nextEntityIdValue(), next_entity_id);
    expectSubtreeMatchesSnapshot(loaded_document, snapshot);

    cad::core::CadDocument fallback_document;
    QString fallback_error = "sentinel";
    CAD_EXPECT(!cad::io::tryLoadCadDocumentFromProject(QJsonObject{}, fallback_document, &fallback_error));
    CAD_EXPECT(fallback_error.isEmpty());
}

CAD_TEST(support_geometry_uses_effective_module_and_enclosure_dimensions)
{
    cad::core::CadDocument document;
    cad::battery::PackLayoutConfig config = makeSingleCellLayoutConfig(cad::battery::CellFormFactor::Cylindrical);
    config.cell_radius = 9.0f;
    config.cell_height = 65.0f;
    cad::battery::PackLayoutGenerator::rebuildDocument(document, config);

    const auto* module0 = findModuleByIndex(document, 0);
    const auto* enclosure0 = findEnclosureByIndex(document, 0);
    CAD_EXPECT(module0 != nullptr);
    CAD_EXPECT(enclosure0 != nullptr);

    const cad::core::EntityId module0_id = module0->id;
    const cad::core::EntityId enclosure0_id = enclosure0->id;

    CAD_EXPECT(document.setModuleBoundaryGeometry(module0_id, module0->center, {180.0f, module0->size.y, 120.0f}));
    CAD_EXPECT(document.setEnclosureGeometry(enclosure0_id, enclosure0->center, {260.0f, enclosure0->size.y, 180.0f}, 11.0f));

    cad::geometry::BatteryGeometryGenerator generator;
    const cad::geometry::GeometryBuffer geometry = generator.buildVisualGeometry(
        document,
        cad::battery::BatteryVisualizationOverlay{},
        nullptr);

    const auto support_bounds = geometryBoundsForLayer(geometry, cad::geometry::SurfaceLayer::Support);
    const auto background_bounds = geometryBoundsForLayer(geometry, cad::geometry::SurfaceLayer::Background);
    CAD_EXPECT(support_bounds.has_value());
    CAD_EXPECT(background_bounds.has_value());

    expectFloatNear(support_bounds->size.x, 180.0f);
    expectFloatNear(support_bounds->size.z, 120.0f);
    expectFloatNear(background_bounds->size.x, 260.0f);
    expectFloatNear(background_bounds->size.z, 180.0f);

    const float outer_half_width = background_bounds->size.x * 0.5f;
    const float inner_half_width = largestInteriorAbsXForLayer(
        geometry,
        cad::geometry::SurfaceLayer::Background,
        background_bounds->center.x,
        outer_half_width);
    expectFloatNear(inner_half_width, outer_half_width - 11.0f);
}

CAD_TEST(imported_binary_stl_mesh_scales_to_current_cell_dimensions)
{
    const QString mesh_path = regressionArtifactPath("binary_box.stl");
    writeBinaryBoxStl(mesh_path, {1.0f, 2.0f, 3.0f}, {2.0f, 4.0f, 6.0f});

    cad::io::TriangleMesh mesh;
    CAD_EXPECT(cad::io::MeshLoader::loadBinaryStl(mesh_path.toStdString(), mesh));
    expectVec3Equals(mesh.source_center, {1.5f, 3.0f, 4.5f});
    expectVec3Equals(mesh.source_size, {1.0f, 2.0f, 3.0f});
    CAD_EXPECT_EQ(mesh.vertices.front().x, 1.0f);
    CAD_EXPECT_NE(mesh.source_size.y, 220.0f);

    cad::core::CadDocument document;
    cad::battery::PackLayoutConfig config = makeSingleCellLayoutConfig(cad::battery::CellFormFactor::Pouch);
    config.cell_width = 40.0f;
    config.cell_height = 80.0f;
    config.cell_depth = 20.0f;
    cad::battery::PackLayoutGenerator::rebuildDocument(document, config);

    const auto* cell00 = findCellByKey(document, 0, 0);
    CAD_EXPECT(cell00 != nullptr);

    cad::geometry::BatteryGeometryGenerator generator;
    const cad::geometry::GeometryBuffer geometry = generator.buildVisualGeometry(
        document,
        cad::battery::BatteryVisualizationOverlay{},
        &mesh);
    const auto cell_geometry_bounds = geometryBoundsForLayer(geometry, cad::geometry::SurfaceLayer::Cell);
    CAD_EXPECT(cell_geometry_bounds.has_value());

    const cad::battery::BoundingBox expected_bounds = document.worldBounds(cell00->id);
    expectVec3Near(cell_geometry_bounds->center, expected_bounds.center);
    expectVec3Near(cell_geometry_bounds->size, expected_bounds.size);
}

CAD_TEST(imported_ascii_stl_mesh_scales_to_current_cylindrical_dimensions)
{
    const QString mesh_path = regressionArtifactPath("ascii_box.stl");
    writeAsciiBoxStl(mesh_path, {-1.0f, -2.0f, -1.0f}, {1.0f, 2.0f, 1.0f});

    cad::io::TriangleMesh mesh;
    CAD_EXPECT(cad::io::MeshLoader::loadBinaryStl(mesh_path.toStdString(), mesh));
    expectVec3Equals(mesh.source_center, {0.0f, 0.0f, 0.0f});
    expectVec3Equals(mesh.source_size, {2.0f, 4.0f, 2.0f});

    cad::core::CadDocument document;
    cad::battery::PackLayoutConfig config = makeSingleCellLayoutConfig(cad::battery::CellFormFactor::Cylindrical);
    config.cell_radius = 9.5f;
    config.cell_height = 70.0f;
    cad::battery::PackLayoutGenerator::rebuildDocument(document, config);

    const auto* cell00 = findCellByKey(document, 0, 0);
    CAD_EXPECT(cell00 != nullptr);

    cad::geometry::BatteryGeometryGenerator generator;
    const cad::geometry::GeometryBuffer geometry = generator.buildVisualGeometry(
        document,
        cad::battery::BatteryVisualizationOverlay{},
        &mesh);
    const auto cell_geometry_bounds = geometryBoundsForLayer(geometry, cad::geometry::SurfaceLayer::Cell);
    CAD_EXPECT(cell_geometry_bounds.has_value());

    const cad::battery::BoundingBox expected_bounds = document.worldBounds(cell00->id);
    expectVec3Near(cell_geometry_bounds->center, expected_bounds.center);
    expectVec3Near(cell_geometry_bounds->size, expected_bounds.size);
}

CAD_TEST(non_cyl_cell_dimension_edit_updates_bounds_and_pickables)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeSingleCellLayoutConfig(cad::battery::CellFormFactor::Prismatic);
    config.layout.cell_width = 28.0f;
    config.layout.cell_depth = 12.0f;
    config.layout.cell_height = 62.0f;
    engine.setBatteryConfig(config);
    engine.setViewportSize(1280, 720);

    const auto* cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(cell00 != nullptr);

    const cad::core::EntityId cell00_id = cell00->id;
    const auto properties = engine.getCellProperties(cell00_id);
    CAD_EXPECT(properties.has_value());
    CAD_EXPECT_EQ(static_cast<int>(properties->form_factor), static_cast<int>(cad::battery::CellFormFactor::Prismatic));
    CAD_EXPECT(!properties->usesRadiusDimension());
    CAD_EXPECT(properties->usesWidthDepthDimensions());

    const cad::battery::BoundingBox initial_bounds = engine.document().worldBounds(cell00_id);
    const auto* initial_pickable = findPickableEntity(engine.renderPacket().pickables, cell00_id);
    CAD_EXPECT(initial_pickable != nullptr);
    const float initial_half_width = initial_pickable->half_width;
    const float initial_half_height = initial_pickable->half_height;

    cad::battery::CellPropertiesUpdate update;
    update.width = properties->width + 18.0f;
    update.depth = properties->depth + 14.0f;
    update.height = properties->height + 22.0f;
    CAD_EXPECT(engine.updateCellProperties(cell00_id, update));

    const auto* updated_cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(updated_cell00 != nullptr);
    CAD_EXPECT_EQ(updated_cell00->width, *update.width);
    CAD_EXPECT_EQ(updated_cell00->depth, *update.depth);
    CAD_EXPECT_EQ(updated_cell00->height, *update.height);

    const cad::battery::BoundingBox updated_bounds = engine.document().worldBounds(cell00_id);
    CAD_EXPECT_EQ(updated_bounds.size.x, *update.width);
    CAD_EXPECT_EQ(updated_bounds.size.y, *update.height);
    CAD_EXPECT_EQ(updated_bounds.size.z, *update.depth);
    CAD_EXPECT_NE(updated_bounds.size.x, initial_bounds.size.x);
    CAD_EXPECT_NE(updated_bounds.size.y, initial_bounds.size.y);
    CAD_EXPECT_NE(updated_bounds.size.z, initial_bounds.size.z);

    const auto* updated_pickable = findPickableEntity(engine.renderPacket().pickables, cell00_id);
    CAD_EXPECT(updated_pickable != nullptr);
    CAD_EXPECT(updated_pickable->half_width > initial_half_width);
    CAD_EXPECT(updated_pickable->half_height > initial_half_height);

    const auto updated_properties = engine.getCellProperties(cell00_id);
    CAD_EXPECT(updated_properties.has_value());
    CAD_EXPECT_EQ(updated_properties->width, *update.width);
    CAD_EXPECT_EQ(updated_properties->depth, *update.depth);
    CAD_EXPECT_EQ(updated_properties->height, *update.height);
    CAD_EXPECT(updated_properties->property_modes.geometry == cad::battery::PropertyMode::UserOverride);
}

CAD_TEST(non_cyl_cell_dimensions_round_trip_through_json)
{
    cad::core::CadDocument document;
    cad::battery::PackLayoutConfig config = makeSingleCellLayoutConfig(cad::battery::CellFormFactor::Pouch);
    config.cell_width = 96.0f;
    config.cell_depth = 8.5f;
    config.cell_height = 108.0f;
    cad::battery::PackLayoutGenerator::rebuildDocument(document, config);

    const auto* cell00 = findCellByKey(document, 0, 0);
    CAD_EXPECT(cell00 != nullptr);
    const cad::core::EntityId cell00_id = cell00->id;

    cad::battery::CellPropertiesUpdate update;
    update.width = 121.5f;
    update.depth = 11.25f;
    update.height = 112.75f;
    CAD_EXPECT(document.updateCellProperties(cell00_id, update));
    document.selectEntity(cell00_id);

    QJsonObject project_root;
    project_root.insert("cad_document", cad::io::serializeCadDocument(document));
    const QJsonDocument json_document(project_root);
    const QJsonDocument reparsed = QJsonDocument::fromJson(json_document.toJson(QJsonDocument::Indented));
    CAD_EXPECT(reparsed.isObject());

    cad::core::CadDocument loaded_document;
    QString load_error;
    CAD_EXPECT(cad::io::tryLoadCadDocumentFromProject(reparsed.object(), loaded_document, &load_error));
    CAD_EXPECT(load_error.isEmpty());
    CAD_EXPECT_EQ(loaded_document.selection().primary, cell00_id);

    const auto* loaded_cell00 = findCellByKey(loaded_document, 0, 0);
    CAD_EXPECT(loaded_cell00 != nullptr);
    CAD_EXPECT_EQ(loaded_cell00->id, cell00_id);
    CAD_EXPECT_EQ(static_cast<int>(loaded_cell00->form_factor), static_cast<int>(cad::battery::CellFormFactor::Pouch));
    CAD_EXPECT_EQ(loaded_cell00->width, *update.width);
    CAD_EXPECT_EQ(loaded_cell00->depth, *update.depth);
    CAD_EXPECT_EQ(loaded_cell00->height, *update.height);
    CAD_EXPECT(loaded_cell00->property_modes.geometry == cad::battery::PropertyMode::UserOverride);
}

CAD_TEST(command_stack_executes_and_reverts_a_simple_cad_command)
{
    cad::CadEngine engine;
    cad::commands::CommandStack stack;

    const cad::core::EntityId pack_id = engine.document().packs().front().id;
    const std::string original_label = engine.document().packs().front().label;
    const std::string renamed_label = "Regression Pack";

    CAD_EXPECT(stack.execute(std::make_unique<cad::commands::RenameEntityCommand>(pack_id, renamed_label), engine));
    CAD_EXPECT(stack.canUndo());
    CAD_EXPECT_EQ(engine.document().findPack(pack_id)->label, renamed_label);

    CAD_EXPECT(stack.undo(engine));
    CAD_EXPECT_EQ(engine.document().findPack(pack_id)->label, original_label);
    CAD_EXPECT(stack.canRedo());

    CAD_EXPECT(stack.redo(engine));
    CAD_EXPECT_EQ(engine.document().findPack(pack_id)->label, renamed_label);
}

CAD_TEST(interactive_move_transaction_commits_one_undoable_command)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(1);
    engine.setBatteryConfig(config);

    const auto* cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(cell00 != nullptr);

    const cad::core::EntityId cell00_id = cell00->id;
    const cad::math::Vec3 original_position = engine.document().worldPosition(cell00_id);
    const cad::math::Vec3 first_delta{8.0f, 0.0f, -6.0f};
    const cad::math::Vec3 second_delta{-3.5f, 2.0f, 4.5f};
    const cad::math::Vec3 total_delta = cad::math::add(first_delta, second_delta);
    const cad::math::Vec3 expected_position = cad::math::add(original_position, total_delta);

    engine.selectEntity(cell00_id);
    CAD_EXPECT_EQ(engine.selectedEntity(), cell00_id);
    CAD_EXPECT(!engine.canUndo());
    CAD_EXPECT(!engine.canRedo());

    CAD_EXPECT(engine.beginInteractiveMove(cell00_id));
    CAD_EXPECT(engine.hasInteractiveMove());
    CAD_EXPECT(engine.updateInteractiveMovePreview(first_delta));
    CAD_EXPECT(engine.updateInteractiveMovePreview(second_delta));
    expectVec3Near(engine.document().worldPosition(cell00_id), expected_position);
    CAD_EXPECT(!engine.canUndo());
    CAD_EXPECT(!engine.canRedo());

    CAD_EXPECT(engine.commitInteractiveMove());
    CAD_EXPECT(!engine.hasInteractiveMove());
    expectVec3Near(engine.document().worldPosition(cell00_id), expected_position);
    CAD_EXPECT_EQ(engine.selectedEntity(), cell00_id);
    CAD_EXPECT(engine.canUndo());
    CAD_EXPECT(!engine.canRedo());

    CAD_EXPECT(engine.undo());
    expectVec3Near(engine.document().worldPosition(cell00_id), original_position);
    CAD_EXPECT_EQ(engine.selectedEntity(), cell00_id);
    CAD_EXPECT(!engine.canUndo());
    CAD_EXPECT(engine.canRedo());

    CAD_EXPECT(engine.redo());
    expectVec3Near(engine.document().worldPosition(cell00_id), expected_position);
    CAD_EXPECT_EQ(engine.selectedEntity(), cell00_id);
    CAD_EXPECT(engine.canUndo());
    CAD_EXPECT(!engine.canRedo());
}

CAD_TEST(interactive_move_transaction_cancel_restores_preview_without_undo_entry)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(1);
    engine.setBatteryConfig(config);

    const auto* group0 = findGroupBySeriesIndex(engine.document(), 0);
    CAD_EXPECT(group0 != nullptr);

    const cad::core::EntityId group0_id = group0->id;
    const cad::math::Vec3 original_position = engine.document().worldPosition(group0_id);
    const cad::math::Vec3 preview_delta{12.0f, 0.0f, 7.5f};

    engine.selectEntity(group0_id);
    CAD_EXPECT(engine.beginInteractiveMove(group0_id));
    CAD_EXPECT(engine.updateInteractiveMovePreview(preview_delta));
    expectVec3Near(
        engine.document().worldPosition(group0_id),
        cad::math::add(original_position, preview_delta));

    CAD_EXPECT(engine.cancelInteractiveMove());
    CAD_EXPECT(!engine.hasInteractiveMove());
    expectVec3Near(engine.document().worldPosition(group0_id), original_position);
    CAD_EXPECT_EQ(engine.selectedEntity(), group0_id);
    CAD_EXPECT(!engine.canUndo());
    CAD_EXPECT(!engine.canRedo());
}

CAD_TEST(render_composer_hides_hidden_module_descendants_from_geometry_pickables_and_selection_overlays)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(2);
    engine.setBatteryConfig(config);
    engine.setViewportSize(1280, 720);

    const auto* module0 = findModuleByIndex(engine.document(), 0);
    const auto* module1 = findModuleByIndex(engine.document(), 1);
    const auto* group4 = findGroupBySeriesIndex(engine.document(), 4);
    const auto* cell41 = findCellByKey(engine.document(), 4, 1);
    const auto* busbar1 = findBusbarByKey(engine.document(), 1, cad::battery::BusbarRole::Negative);
    const auto* plate1 = findCoolingPlateByIndex(engine.document(), 1);

    CAD_EXPECT(module0 != nullptr);
    CAD_EXPECT(module1 != nullptr);
    CAD_EXPECT(group4 != nullptr);
    CAD_EXPECT(cell41 != nullptr);
    CAD_EXPECT(busbar1 != nullptr);
    CAD_EXPECT(plate1 != nullptr);

    const cad::core::EntityId module0_id = module0->id;
    const cad::core::EntityId module1_id = module1->id;
    const cad::core::EntityId group4_id = group4->id;
    const cad::core::EntityId cell41_id = cell41->id;
    const cad::core::EntityId busbar1_id = busbar1->id;
    const cad::core::EntityId plate1_id = plate1->id;

    engine.selectEntity(cell41_id);
    CAD_EXPECT(!engine.renderPacket().selection_overlay_lines.empty());

    const std::size_t visible_triangle_count = engine.visualGeometry().triangles.size();
    const std::size_t visible_pickable_count = engine.renderPacket().pickables.size();
    CAD_EXPECT(containsPickableEntity(engine.renderPacket().pickables, module1_id));
    CAD_EXPECT(containsPickableEntity(engine.renderPacket().pickables, group4_id));
    CAD_EXPECT(containsPickableEntity(engine.renderPacket().pickables, cell41_id));
    CAD_EXPECT(containsPickableEntity(engine.renderPacket().pickables, busbar1_id));
    CAD_EXPECT(containsPickableEntity(engine.renderPacket().pickables, plate1_id));

    CAD_EXPECT(engine.setEntityVisibility(module1_id, false));

    CAD_EXPECT(!engine.document().isEffectivelyVisible(module1_id));
    CAD_EXPECT(!engine.document().isEffectivelyVisible(group4_id));
    CAD_EXPECT(!engine.document().isEffectivelyVisible(cell41_id));
    CAD_EXPECT(!engine.document().isEffectivelyVisible(busbar1_id));
    CAD_EXPECT(!engine.document().isEffectivelyVisible(plate1_id));
    CAD_EXPECT(engine.document().isEffectivelyVisible(module0_id));

    CAD_EXPECT(engine.visualGeometry().triangles.size() < visible_triangle_count);
    CAD_EXPECT(engine.renderPacket().pickables.size() < visible_pickable_count);
    CAD_EXPECT(containsPickableEntity(engine.renderPacket().pickables, module0_id));
    CAD_EXPECT(!containsPickableEntity(engine.renderPacket().pickables, module1_id));
    CAD_EXPECT(!containsPickableEntity(engine.renderPacket().pickables, group4_id));
    CAD_EXPECT(!containsPickableEntity(engine.renderPacket().pickables, cell41_id));
    CAD_EXPECT(!containsPickableEntity(engine.renderPacket().pickables, busbar1_id));
    CAD_EXPECT(!containsPickableEntity(engine.renderPacket().pickables, plate1_id));
    CAD_EXPECT(engine.renderPacket().selection_overlay_lines.empty());
}

CAD_TEST(hit_tester_prefers_nearest_pickable_then_priority_on_equal_depth)
{
    cad::render::RenderPacket packet;
    packet.pickables.push_back({
        cad::core::EntityId{101},
        cad::render::ScreenPickable::Shape::Rectangle,
        1,
        64.0f,
        48.0f,
        10.0f,
        8.0f,
        0.2f
    });
    packet.pickables.push_back({
        cad::core::EntityId{202},
        cad::render::ScreenPickable::Shape::Circle,
        3,
        64.0f,
        48.0f,
        8.0f,
        8.0f,
        0.4f
    });
    packet.pickables.push_back({
        cad::core::EntityId{303},
        cad::render::ScreenPickable::Shape::Circle,
        3,
        64.0f,
        48.0f,
        8.0f,
        8.0f,
        0.1f
    });
    packet.pickables.push_back({
        cad::core::EntityId{404},
        cad::render::ScreenPickable::Shape::Circle,
        9,
        64.0f,
        48.0f,
        8.0f,
        8.0f,
        0.6f
    });

    cad::picking::HitTester hit_tester;

    CAD_EXPECT_EQ(hit_tester.hitTestEntity(packet, 64.0f, 48.0f), cad::core::EntityId{303});
    CAD_EXPECT_EQ(hit_tester.hitTestEntity(packet, 200.0f, 200.0f), cad::core::EntityId{});

    cad::render::RenderPacket equal_depth_packet;
    equal_depth_packet.pickables.push_back({
        cad::core::EntityId{505},
        cad::render::ScreenPickable::Shape::Rectangle,
        2,
        30.0f,
        30.0f,
        12.0f,
        12.0f,
        0.15f
    });
    equal_depth_packet.pickables.push_back({
        cad::core::EntityId{606},
        cad::render::ScreenPickable::Shape::Rectangle,
        8,
        30.0f,
        30.0f,
        12.0f,
        12.0f,
        0.15f
    });

    CAD_EXPECT_EQ(hit_tester.hitTestEntity(equal_depth_packet, 30.0f, 30.0f), cad::core::EntityId{606});
}

CAD_TEST(remove_entity_command_restores_module_subtree_across_undo_redo)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(2);
    engine.setBatteryConfig(config);

    const auto* module1 = findModuleByIndex(engine.document(), 1);
    const auto* module0 = findModuleByIndex(engine.document(), 0);
    const auto* group4 = findGroupBySeriesIndex(engine.document(), 4);
    const auto* cell41 = findCellByKey(engine.document(), 4, 1);
    const auto* negative_busbar1 = findBusbarByKey(engine.document(), 1, cad::battery::BusbarRole::Negative);
    const auto* cooling_plate1 = findCoolingPlateByIndex(engine.document(), 1);

    CAD_EXPECT(module1 != nullptr);
    CAD_EXPECT(module0 != nullptr);
    CAD_EXPECT(group4 != nullptr);
    CAD_EXPECT(cell41 != nullptr);
    CAD_EXPECT(negative_busbar1 != nullptr);
    CAD_EXPECT(cooling_plate1 != nullptr);

    const cad::core::EntityId module1_id = module1->id;
    const cad::core::EntityId module0_id = module0->id;
    const cad::core::EntityId group4_id = group4->id;
    const cad::core::EntityId cell41_id = cell41->id;
    const cad::core::EntityId negative_busbar1_id = negative_busbar1->id;
    const cad::core::EntityId cooling_plate1_id = cooling_plate1->id;

    CAD_EXPECT(engine.setEntityLabel(module1_id, "Module 2 override"));
    CAD_EXPECT(engine.setEntityVisibility(group4_id, false));
    CAD_EXPECT(engine.setCellPosition(cell41_id, {11.0f, 12.0f, 13.0f}));
    CAD_EXPECT(engine.setCellGeometry(cell41_id, 12.25f, 71.5f));
    CAD_EXPECT(engine.setBusbarGeometry(negative_busbar1_id, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}));
    CAD_EXPECT(engine.setCoolingPlateGeometry(cooling_plate1_id, {7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}));

    const std::vector<cad::battery::EntityRecord> module_snapshot = captureSubtreeSnapshot(engine.document(), module1_id);
    CAD_EXPECT(!module_snapshot.empty());
    CAD_EXPECT(engine.document().hasEntity(module0_id));

    engine.selectEntity(cell41_id);
    CAD_EXPECT_EQ(engine.selectedEntity(), cell41_id);
    CAD_EXPECT(engine.applyRemoveEntity(module1_id));

    expectSubtreeRemoved(engine.document(), module_snapshot);
    CAD_EXPECT(engine.document().hasEntity(module0_id));
    CAD_EXPECT(!engine.selectedEntity().isValid());
    CAD_EXPECT(engine.undo());

    expectSubtreeMatchesSnapshot(engine.document(), module_snapshot);
    CAD_EXPECT_EQ(engine.selectedEntity(), cell41_id);
    CAD_EXPECT(engine.redo());

    expectSubtreeRemoved(engine.document(), module_snapshot);
    CAD_EXPECT(engine.document().hasEntity(module0_id));
    CAD_EXPECT(!engine.selectedEntity().isValid());
    CAD_EXPECT(engine.undo());

    expectSubtreeMatchesSnapshot(engine.document(), module_snapshot);
    CAD_EXPECT_EQ(engine.selectedEntity(), cell41_id);
}

CAD_TEST(remove_entity_command_restores_group_subtree_without_disturbing_valid_selection)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(1);
    engine.setBatteryConfig(config);

    const auto* module0 = findModuleByIndex(engine.document(), 0);
    const auto* group0 = findGroupBySeriesIndex(engine.document(), 0);
    const auto* cell00 = findCellByKey(engine.document(), 0, 0);
    const auto* cell01 = findCellByKey(engine.document(), 0, 1);

    CAD_EXPECT(module0 != nullptr);
    CAD_EXPECT(group0 != nullptr);
    CAD_EXPECT(cell00 != nullptr);
    CAD_EXPECT(cell01 != nullptr);

    const cad::core::EntityId module0_id = module0->id;
    const cad::core::EntityId group0_id = group0->id;
    const cad::core::EntityId cell00_id = cell00->id;
    const cad::core::EntityId cell01_id = cell01->id;

    CAD_EXPECT(engine.setEntityLabel(group0_id, "Series 1 override"));
    CAD_EXPECT(engine.setEntityPosition(group0_id, {2.0f, 3.0f, 4.0f}));
    CAD_EXPECT(engine.setEntityVisibility(group0_id, false));
    CAD_EXPECT(engine.setCellPosition(cell00_id, {5.0f, 6.0f, 7.0f}));
    CAD_EXPECT(engine.setCellGeometry(cell01_id, 13.0f, 74.0f));

    const std::vector<cad::battery::EntityRecord> group_snapshot = captureSubtreeSnapshot(engine.document(), group0_id);
    CAD_EXPECT_EQ(group_snapshot.size(), std::size_t{3});

    engine.selectEntity(module0_id);
    CAD_EXPECT_EQ(engine.selectedEntity(), module0_id);
    CAD_EXPECT(engine.applyRemoveEntity(group0_id));

    expectSubtreeRemoved(engine.document(), group_snapshot);
    CAD_EXPECT(engine.document().hasEntity(module0_id));
    CAD_EXPECT_EQ(engine.selectedEntity(), module0_id);
    CAD_EXPECT(engine.undo());

    expectSubtreeMatchesSnapshot(engine.document(), group_snapshot);
    CAD_EXPECT_EQ(engine.selectedEntity(), module0_id);
    CAD_EXPECT(engine.redo());

    expectSubtreeRemoved(engine.document(), group_snapshot);
    CAD_EXPECT(engine.document().hasEntity(module0_id));
    CAD_EXPECT_EQ(engine.selectedEntity(), module0_id);
}

CAD_TEST(pack_layout_regeneration_rebuilds_topology_without_stale_parent_links)
{
    cad::core::CadDocument document;
    const cad::battery::PackLayoutConfig single_module = makeLayoutConfig(1);
    const cad::battery::PackLayoutConfig multi_module = makeLayoutConfig(2);

    cad::battery::PackLayoutGenerator::rebuildDocument(document, single_module);

    const auto* initial_module0 = findModuleByIndex(document, 0);
    const auto* initial_group0 = findGroupBySeriesIndex(document, 0);
    const auto* initial_group4 = findGroupBySeriesIndex(document, 4);
    const auto* initial_cell41 = findCellByKey(document, 4, 1);
    const auto* initial_negative_busbar0 = findBusbarByKey(document, 0, cad::battery::BusbarRole::Negative);
    const auto* initial_positive_busbar0 = findBusbarByKey(document, 0, cad::battery::BusbarRole::Positive);
    const auto* initial_cooling_plate0 = findCoolingPlateByIndex(document, 0);
    const auto* initial_enclosure0 = findEnclosureByIndex(document, 0);

    CAD_EXPECT(initial_module0 != nullptr);
    CAD_EXPECT(initial_group0 != nullptr);
    CAD_EXPECT(initial_group4 != nullptr);
    CAD_EXPECT(initial_cell41 != nullptr);
    CAD_EXPECT(initial_negative_busbar0 != nullptr);
    CAD_EXPECT(initial_positive_busbar0 != nullptr);
    CAD_EXPECT(initial_cooling_plate0 != nullptr);
    CAD_EXPECT(initial_enclosure0 != nullptr);

    const cad::core::EntityId pack_id = document.packs().front().id;
    const cad::core::EntityId module0_id = initial_module0->id;
    const cad::core::EntityId group0_id = initial_group0->id;
    const cad::core::EntityId group4_id = initial_group4->id;
    const cad::core::EntityId cell41_id = initial_cell41->id;
    const cad::core::EntityId negative_busbar0_id = initial_negative_busbar0->id;
    const cad::core::EntityId positive_busbar0_id = initial_positive_busbar0->id;
    const cad::core::EntityId cooling_plate0_id = initial_cooling_plate0->id;
    const cad::core::EntityId enclosure_id = initial_enclosure0->id;

    std::map<int, cad::core::EntityId> original_group_ids;
    for (const auto& group : document.cellGroups()) {
        original_group_ids[group.series_index] = group.id;
    }

    std::map<CellKey, cad::core::EntityId> original_cell_ids;
    for (const auto& cell : document.cells()) {
        original_cell_ids[{cell.series_index, cell.parallel_index}] = cell.id;
    }

    std::map<BusbarKey, cad::core::EntityId> original_busbar_ids;
    for (const auto& busbar : document.busbars()) {
        original_busbar_ids[{busbar.module_index, busbar.role}] = busbar.id;
    }

    std::map<int, cad::core::EntityId> original_cooling_plate_ids;
    for (const auto& plate : document.coolingPlates()) {
        original_cooling_plate_ids[plate.plate_index] = plate.id;
    }

    CAD_EXPECT(document.setEntityLabel(group4_id, "Series 5 group"));
    CAD_EXPECT(document.setEntityVisibility(enclosure_id, false));
    CAD_EXPECT(document.setCellPosition(cell41_id, {4.0f, 5.0f, 6.0f}));
    CAD_EXPECT(document.setCellGeometry(cell41_id, 12.5f, 73.0f));
    CAD_EXPECT(document.setBusbarGeometry(negative_busbar0_id, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}));
    CAD_EXPECT(document.setCoolingPlateGeometry(cooling_plate0_id, {7.0f, 8.0f, 9.0f}, {10.0f, 11.0f, 12.0f}));
    CAD_EXPECT(document.setModuleBoundaryGeometry(module0_id, {13.0f, 14.0f, 15.0f}, {16.0f, 17.0f, 18.0f}));
    CAD_EXPECT(document.setEnclosureGeometry(enclosure_id, {19.0f, 20.0f, 21.0f}, {22.0f, 23.0f, 24.0f}, 2.5f));

    corruptParent(document, pack_id, module0_id);
    corruptParent(document, module0_id, group4_id);
    corruptParent(document, group4_id, pack_id);
    corruptParent(document, cell41_id, group0_id);
    corruptParent(document, negative_busbar0_id, pack_id);
    corruptParent(document, cooling_plate0_id, pack_id);
    corruptParent(document, enclosure_id, module0_id);

    cad::battery::PackLayoutGenerator::rebuildDocument(document, multi_module);
    expectHierarchyMatchesLayout(document, multi_module);

    CAD_EXPECT_EQ(document.packs().front().id, pack_id);
    CAD_EXPECT(!document.packs().front().parent_id.isValid());

    const auto* module0 = findModuleByIndex(document, 0);
    const auto* module1 = findModuleByIndex(document, 1);
    const auto* group4 = findGroupBySeriesIndex(document, 4);
    const auto* cell41 = findCellByKey(document, 4, 1);
    const auto* negative_busbar0 = findBusbarByKey(document, 0, cad::battery::BusbarRole::Negative);
    const auto* positive_busbar0 = findBusbarByKey(document, 0, cad::battery::BusbarRole::Positive);
    const auto* cooling_plate0 = findCoolingPlateByIndex(document, 0);
    const auto* enclosure0 = findEnclosureByIndex(document, 0);

    CAD_EXPECT(module0 != nullptr);
    CAD_EXPECT(module1 != nullptr);
    CAD_EXPECT(group4 != nullptr);
    CAD_EXPECT(cell41 != nullptr);
    CAD_EXPECT(negative_busbar0 != nullptr);
    CAD_EXPECT(positive_busbar0 != nullptr);
    CAD_EXPECT(cooling_plate0 != nullptr);
    CAD_EXPECT(enclosure0 != nullptr);

    CAD_EXPECT_EQ(module0->id, module0_id);
    CAD_EXPECT_EQ(module0->center.x, 13.0f);
    CAD_EXPECT_EQ(module0->center.y, 14.0f);
    CAD_EXPECT_EQ(module0->center.z, 15.0f);
    CAD_EXPECT_EQ(module0->size.x, 16.0f);
    CAD_EXPECT_EQ(module0->size.y, 17.0f);
    CAD_EXPECT_EQ(module0->size.z, 18.0f);

    for (const auto& [series_index, expected_id] : original_group_ids) {
        const auto* group = findGroupBySeriesIndex(document, series_index);
        CAD_EXPECT(group != nullptr);
        CAD_EXPECT_EQ(group->id, expected_id);
    }

    for (const auto& [cell_key, expected_id] : original_cell_ids) {
        const auto* cell = findCellByKey(document, cell_key.first, cell_key.second);
        CAD_EXPECT(cell != nullptr);
        CAD_EXPECT_EQ(cell->id, expected_id);
    }

    for (const auto& [busbar_key, expected_id] : original_busbar_ids) {
        const auto* busbar = findBusbarByKey(document, busbar_key.first, busbar_key.second);
        CAD_EXPECT(busbar != nullptr);
        CAD_EXPECT_EQ(busbar->id, expected_id);
    }

    for (const auto& [plate_index, expected_id] : original_cooling_plate_ids) {
        const auto* plate = findCoolingPlateByIndex(document, plate_index);
        CAD_EXPECT(plate != nullptr);
        CAD_EXPECT_EQ(plate->id, expected_id);
    }

    CAD_EXPECT_EQ(group4->id, group4_id);
    CAD_EXPECT_EQ(group4->label, std::string("Series 5 group"));
    CAD_EXPECT_EQ(group4->parent_id, module1->id);

    CAD_EXPECT_EQ(cell41->id, cell41_id);
    CAD_EXPECT_EQ(cell41->position.x, 4.0f);
    CAD_EXPECT_EQ(cell41->position.y, 5.0f);
    CAD_EXPECT_EQ(cell41->position.z, 6.0f);
    CAD_EXPECT_EQ(cell41->radius, 12.5f);
    CAD_EXPECT_EQ(cell41->height, 73.0f);
    CAD_EXPECT_EQ(cell41->parent_id, group4->id);

    CAD_EXPECT_EQ(negative_busbar0->id, negative_busbar0_id);
    CAD_EXPECT_EQ(negative_busbar0->module_index, 0);
    CAD_EXPECT_EQ(negative_busbar0->center.x, 1.0f);
    CAD_EXPECT_EQ(negative_busbar0->center.y, 2.0f);
    CAD_EXPECT_EQ(negative_busbar0->center.z, 3.0f);
    CAD_EXPECT_EQ(negative_busbar0->size.x, 4.0f);
    CAD_EXPECT_EQ(negative_busbar0->size.y, 5.0f);
    CAD_EXPECT_EQ(negative_busbar0->size.z, 6.0f);
    CAD_EXPECT_EQ(negative_busbar0->parent_id, module0->id);

    CAD_EXPECT_EQ(positive_busbar0->id, positive_busbar0_id);
    CAD_EXPECT_EQ(positive_busbar0->parent_id, module0->id);

    CAD_EXPECT_EQ(cooling_plate0->id, cooling_plate0_id);
    CAD_EXPECT_EQ(cooling_plate0->center.x, 7.0f);
    CAD_EXPECT_EQ(cooling_plate0->center.y, 8.0f);
    CAD_EXPECT_EQ(cooling_plate0->center.z, 9.0f);
    CAD_EXPECT_EQ(cooling_plate0->size.x, 10.0f);
    CAD_EXPECT_EQ(cooling_plate0->size.y, 11.0f);
    CAD_EXPECT_EQ(cooling_plate0->size.z, 12.0f);
    CAD_EXPECT_EQ(cooling_plate0->parent_id, module0->id);

    CAD_EXPECT_EQ(enclosure0->id, enclosure_id);
    CAD_EXPECT_EQ(enclosure0->center.x, 19.0f);
    CAD_EXPECT_EQ(enclosure0->center.y, 20.0f);
    CAD_EXPECT_EQ(enclosure0->center.z, 21.0f);
    CAD_EXPECT_EQ(enclosure0->size.x, 22.0f);
    CAD_EXPECT_EQ(enclosure0->size.y, 23.0f);
    CAD_EXPECT_EQ(enclosure0->size.z, 24.0f);
    CAD_EXPECT_EQ(enclosure0->wall_thickness, 2.5f);
    CAD_EXPECT_EQ(enclosure0->visible, false);
    CAD_EXPECT_EQ(enclosure0->parent_id, pack_id);

    document.selectEntity(module1->id);
    cad::battery::PackLayoutGenerator::rebuildDocument(document, single_module);
    expectHierarchyMatchesLayout(document, single_module);

    CAD_EXPECT(!document.selection().primary.isValid());

    const auto* restored_module0 = findModuleByIndex(document, 0);
    const auto* restored_group4 = findGroupBySeriesIndex(document, 4);
    const auto* restored_cell41 = findCellByKey(document, 4, 1);
    const auto* restored_negative_busbar0 = findBusbarByKey(document, 0, cad::battery::BusbarRole::Negative);
    const auto* restored_positive_busbar0 = findBusbarByKey(document, 0, cad::battery::BusbarRole::Positive);
    const auto* restored_cooling_plate0 = findCoolingPlateByIndex(document, 0);
    const auto* restored_enclosure0 = findEnclosureByIndex(document, 0);

    CAD_EXPECT(restored_module0 != nullptr);
    CAD_EXPECT(restored_group4 != nullptr);
    CAD_EXPECT(restored_cell41 != nullptr);
    CAD_EXPECT(restored_negative_busbar0 != nullptr);
    CAD_EXPECT(restored_positive_busbar0 != nullptr);
    CAD_EXPECT(restored_cooling_plate0 != nullptr);
    CAD_EXPECT(restored_enclosure0 != nullptr);
    CAD_EXPECT(findModuleByIndex(document, 1) == nullptr);
    CAD_EXPECT(findCoolingPlateByIndex(document, 1) == nullptr);
    CAD_EXPECT(findBusbarByKey(document, 1, cad::battery::BusbarRole::Negative) == nullptr);
    CAD_EXPECT(findBusbarByKey(document, 1, cad::battery::BusbarRole::Positive) == nullptr);

    CAD_EXPECT_EQ(document.packs().front().id, pack_id);
    CAD_EXPECT_EQ(restored_module0->id, module0_id);
    CAD_EXPECT_EQ(restored_module0->center.x, 13.0f);
    CAD_EXPECT_EQ(restored_module0->center.y, 14.0f);
    CAD_EXPECT_EQ(restored_module0->center.z, 15.0f);
    CAD_EXPECT_EQ(restored_module0->size.x, 16.0f);
    CAD_EXPECT_EQ(restored_module0->size.y, 17.0f);
    CAD_EXPECT_EQ(restored_module0->size.z, 18.0f);

    for (const auto& [series_index, expected_id] : original_group_ids) {
        const auto* group = findGroupBySeriesIndex(document, series_index);
        CAD_EXPECT(group != nullptr);
        CAD_EXPECT_EQ(group->id, expected_id);
    }

    for (const auto& [cell_key, expected_id] : original_cell_ids) {
        const auto* cell = findCellByKey(document, cell_key.first, cell_key.second);
        CAD_EXPECT(cell != nullptr);
        CAD_EXPECT_EQ(cell->id, expected_id);
    }

    CAD_EXPECT_EQ(restored_group4->id, group4_id);
    CAD_EXPECT_EQ(restored_group4->label, std::string("Series 5 group"));
    CAD_EXPECT_EQ(restored_cell41->id, cell41_id);
    CAD_EXPECT_EQ(restored_cell41->position.x, 4.0f);
    CAD_EXPECT_EQ(restored_cell41->position.y, 5.0f);
    CAD_EXPECT_EQ(restored_cell41->position.z, 6.0f);
    CAD_EXPECT_EQ(restored_cell41->radius, 12.5f);
    CAD_EXPECT_EQ(restored_cell41->height, 73.0f);

    CAD_EXPECT_EQ(restored_negative_busbar0->id, negative_busbar0_id);
    CAD_EXPECT_EQ(restored_negative_busbar0->center.x, 1.0f);
    CAD_EXPECT_EQ(restored_negative_busbar0->center.y, 2.0f);
    CAD_EXPECT_EQ(restored_negative_busbar0->center.z, 3.0f);
    CAD_EXPECT_EQ(restored_negative_busbar0->size.x, 4.0f);
    CAD_EXPECT_EQ(restored_negative_busbar0->size.y, 5.0f);
    CAD_EXPECT_EQ(restored_negative_busbar0->size.z, 6.0f);
    CAD_EXPECT_EQ(restored_positive_busbar0->id, positive_busbar0_id);
    CAD_EXPECT_EQ(restored_cooling_plate0->id, cooling_plate0_id);
    CAD_EXPECT_EQ(restored_cooling_plate0->center.x, 7.0f);
    CAD_EXPECT_EQ(restored_cooling_plate0->center.y, 8.0f);
    CAD_EXPECT_EQ(restored_cooling_plate0->center.z, 9.0f);
    CAD_EXPECT_EQ(restored_cooling_plate0->size.x, 10.0f);
    CAD_EXPECT_EQ(restored_cooling_plate0->size.y, 11.0f);
    CAD_EXPECT_EQ(restored_cooling_plate0->size.z, 12.0f);
    CAD_EXPECT_EQ(restored_enclosure0->id, enclosure_id);
    CAD_EXPECT_EQ(restored_enclosure0->visible, false);
    CAD_EXPECT_EQ(restored_enclosure0->center.x, 19.0f);
    CAD_EXPECT_EQ(restored_enclosure0->center.y, 20.0f);
    CAD_EXPECT_EQ(restored_enclosure0->center.z, 21.0f);
    CAD_EXPECT_EQ(restored_enclosure0->size.x, 22.0f);
    CAD_EXPECT_EQ(restored_enclosure0->size.y, 23.0f);
    CAD_EXPECT_EQ(restored_enclosure0->size.z, 24.0f);
    CAD_EXPECT_EQ(restored_enclosure0->wall_thickness, 2.5f);
}

CAD_TEST(simulation_mapping_prefers_group_topology_and_ignores_visibility)
{
    cad::core::CadDocument document;
    cad::battery::PackLayoutConfig config = makeLayoutConfig(2);
    cad::battery::PackLayoutGenerator::rebuildDocument(document, config);

    const auto* group0 = findGroupBySeriesIndex(document, 0);
    const auto* group4 = findGroupBySeriesIndex(document, 4);
    const auto* cell00 = findCellByKey(document, 0, 0);
    const auto* cell41 = findCellByKey(document, 4, 1);

    CAD_EXPECT(group0 != nullptr);
    CAD_EXPECT(group4 != nullptr);
    CAD_EXPECT(cell00 != nullptr);
    CAD_EXPECT(cell41 != nullptr);

    document.metadata().layout_config.cells_in_series = 99;
    CAD_EXPECT(document.setEntityLabel(group4->id, "Mid-pack Series"));

    const auto expected_groups = orderedGroupsBySeries(document);
    const auto visible_mapping = SimulationMappingBuilder::build(document, 24.0, 6.5);

    CAD_EXPECT_EQ(visible_mapping.groupCount, static_cast<int>(expected_groups.size()));
    CAD_EXPECT_EQ(visible_mapping.groupCount, static_cast<int>(document.cellGroups().size()));
    CAD_EXPECT_EQ(visible_mapping.groupEntityIds.size(), static_cast<qsizetype>(expected_groups.size()));
    CAD_EXPECT_EQ(visible_mapping.groupLabels.size(), static_cast<qsizetype>(expected_groups.size()));

    for (qsizetype index = 0; index < visible_mapping.groupEntityIds.size(); ++index) {
        const auto* expected_group = expected_groups[static_cast<std::size_t>(index)];
        const std::string expected_label = (expected_group->label.empty()
            ? QString("Series Group %1").arg(expected_group->series_index + 1)
            : QString::fromStdString(expected_group->label)).toStdString();

        CAD_EXPECT_EQ(visible_mapping.groupEntityIds.at(index).toString().toStdString(), entityIdString(expected_group->id));
        CAD_EXPECT_EQ(visible_mapping.groupLabels.at(index).toString().toStdString(), expected_label);
    }

    CAD_EXPECT(document.setEntityVisibility(group4->id, false));
    CAD_EXPECT(document.setEntityVisibility(cell00->id, false));
    CAD_EXPECT(document.setEntityVisibility(cell41->id, false));

    const auto hidden_mapping = SimulationMappingBuilder::build(document, 24.0, 6.5);

    CAD_EXPECT_EQ(hidden_mapping.groupCount, visible_mapping.groupCount);
    CAD_EXPECT_EQ(hidden_mapping.groupEntityIds.size(), visible_mapping.groupEntityIds.size());
    CAD_EXPECT_EQ(hidden_mapping.groupLabels.size(), visible_mapping.groupLabels.size());
    CAD_EXPECT_EQ(hidden_mapping.groupZoneAssignments.size(), visible_mapping.groupZoneAssignments.size());

    for (qsizetype index = 0; index < hidden_mapping.groupEntityIds.size(); ++index) {
        CAD_EXPECT_EQ(hidden_mapping.groupEntityIds.at(index).toString().toStdString(), visible_mapping.groupEntityIds.at(index).toString().toStdString());
        CAD_EXPECT_EQ(hidden_mapping.groupLabels.at(index).toString().toStdString(), visible_mapping.groupLabels.at(index).toString().toStdString());
        CAD_EXPECT_EQ(hidden_mapping.groupZoneAssignments.at(index).toInt(), visible_mapping.groupZoneAssignments.at(index).toInt());
    }
}

CAD_TEST(reset_to_generated_restores_current_module_center)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(2);
    engine.setBatteryConfig(config);

    const auto* module1 = findModuleByIndex(engine.document(), 1);
    CAD_EXPECT(module1 != nullptr);

    const cad::core::EntityId module1_id = module1->id;
    const cad::math::Vec3 moved_center{123.0f, 45.0f, 67.0f};

    CAD_EXPECT(engine.setEntityPosition(module1_id, moved_center));
    module1 = findModuleByIndex(engine.document(), 1);
    CAD_EXPECT(module1 != nullptr);
    expectVec3Equals(module1->center, moved_center);
    CAD_EXPECT(module1->property_modes.position == cad::battery::PropertyMode::UserOverride);

    cad::battery::BatteryCadConfig updated = config;
    updated.layout.module_gap_x += 31.0f;
    updated.layout.module_tray_margin_x += 4.0f;
    engine.setBatteryConfig(updated);

    const auto* overridden_module1 = findModuleByIndex(engine.document(), 1);
    CAD_EXPECT(overridden_module1 != nullptr);
    CAD_EXPECT_EQ(overridden_module1->id, module1_id);
    expectVec3Equals(overridden_module1->center, moved_center);
    CAD_EXPECT(overridden_module1->property_modes.position == cad::battery::PropertyMode::UserOverride);

    const cad::core::CadDocument generated = buildGeneratedDocument(updated.layout);
    const auto* expected_module1 = findModuleByIndex(generated, 1);
    CAD_EXPECT(expected_module1 != nullptr);
    CAD_EXPECT_NE(expected_module1->center.x, moved_center.x);

    CAD_EXPECT(engine.resetEntityPositionToGenerated(module1_id));

    const auto* reset_module1 = findModuleByIndex(engine.document(), 1);
    CAD_EXPECT(reset_module1 != nullptr);
    CAD_EXPECT_EQ(reset_module1->id, module1_id);
    expectVec3Equals(reset_module1->center, expected_module1->center);
    CAD_EXPECT(reset_module1->property_modes.position == cad::battery::PropertyMode::Generated);
}

CAD_TEST(reset_to_generated_restores_generator_label_naming)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(2);
    engine.setBatteryConfig(config);

    const auto* module1 = findModuleByIndex(engine.document(), 1);
    const auto* plate1 = findCoolingPlateByIndex(engine.document(), 1);
    CAD_EXPECT(module1 != nullptr);
    CAD_EXPECT(plate1 != nullptr);

    const cad::core::EntityId module1_id = module1->id;
    const cad::core::EntityId plate1_id = plate1->id;

    CAD_EXPECT(engine.setEntityLabel(module1_id, "Module override"));
    CAD_EXPECT(engine.setEntityLabel(plate1_id, "Channel override"));

    module1 = findModuleByIndex(engine.document(), 1);
    plate1 = findCoolingPlateByIndex(engine.document(), 1);
    CAD_EXPECT(module1 != nullptr);
    CAD_EXPECT(plate1 != nullptr);
    CAD_EXPECT_EQ(module1->label, std::string("Module override"));
    CAD_EXPECT_EQ(plate1->label, std::string("Channel override"));
    CAD_EXPECT(module1->property_modes.label == cad::battery::PropertyMode::UserOverride);
    CAD_EXPECT(plate1->property_modes.label == cad::battery::PropertyMode::UserOverride);

    const cad::core::CadDocument generated = buildGeneratedDocument(config.layout);
    const auto* expected_module1 = findModuleByIndex(generated, 1);
    const auto* expected_plate1 = findCoolingPlateByIndex(generated, 1);
    CAD_EXPECT(expected_module1 != nullptr);
    CAD_EXPECT(expected_plate1 != nullptr);
    CAD_EXPECT_EQ(expected_module1->label, std::string("Battery module 2"));
    CAD_EXPECT_EQ(expected_plate1->label, std::string("Cooling plate 2"));

    CAD_EXPECT(engine.resetEntityLabelToGenerated(module1_id));
    CAD_EXPECT(engine.resetEntityLabelToGenerated(plate1_id));

    const auto* reset_module1 = findModuleByIndex(engine.document(), 1);
    const auto* reset_plate1 = findCoolingPlateByIndex(engine.document(), 1);
    CAD_EXPECT(reset_module1 != nullptr);
    CAD_EXPECT(reset_plate1 != nullptr);
    CAD_EXPECT_EQ(reset_module1->label, expected_module1->label);
    CAD_EXPECT_EQ(reset_plate1->label, expected_plate1->label);
    CAD_EXPECT(reset_module1->property_modes.label == cad::battery::PropertyMode::Generated);
    CAD_EXPECT(reset_plate1->property_modes.label == cad::battery::PropertyMode::Generated);
}

CAD_TEST(reset_to_generated_restores_current_generated_geometry)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeLayoutConfig(1);
    engine.setBatteryConfig(config);

    const auto* cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(cell00 != nullptr);

    const cad::core::EntityId cell00_id = cell00->id;
    CAD_EXPECT(engine.setCellGeometry(cell00_id, 12.5f, 71.0f));

    cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(cell00 != nullptr);
    CAD_EXPECT_EQ(cell00->radius, 12.5f);
    CAD_EXPECT_EQ(cell00->height, 71.0f);
    CAD_EXPECT(cell00->property_modes.geometry == cad::battery::PropertyMode::UserOverride);

    cad::battery::BatteryCadConfig updated = config;
    updated.layout.cell_radius = 9.75f;
    updated.layout.cell_height = 83.0f;
    updated.layout.x_spacing += 6.0f;
    engine.setBatteryConfig(updated);

    const auto* overridden_cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(overridden_cell00 != nullptr);
    CAD_EXPECT_EQ(overridden_cell00->id, cell00_id);
    CAD_EXPECT_EQ(overridden_cell00->radius, 12.5f);
    CAD_EXPECT_EQ(overridden_cell00->height, 71.0f);
    CAD_EXPECT(overridden_cell00->property_modes.geometry == cad::battery::PropertyMode::UserOverride);

    const cad::core::CadDocument generated = buildGeneratedDocument(updated.layout);
    const auto* expected_cell00 = findCellByKey(generated, 0, 0);
    CAD_EXPECT(expected_cell00 != nullptr);
    CAD_EXPECT_NE(expected_cell00->radius, overridden_cell00->radius);
    CAD_EXPECT_NE(expected_cell00->height, overridden_cell00->height);

    CAD_EXPECT(engine.resetEntityGeometryToGenerated(cell00_id));

    const auto* reset_cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(reset_cell00 != nullptr);
    CAD_EXPECT_EQ(reset_cell00->id, cell00_id);
    CAD_EXPECT_EQ(static_cast<int>(reset_cell00->form_factor), static_cast<int>(expected_cell00->form_factor));
    CAD_EXPECT_EQ(reset_cell00->radius, expected_cell00->radius);
    CAD_EXPECT_EQ(reset_cell00->height, expected_cell00->height);
    CAD_EXPECT_EQ(reset_cell00->width, expected_cell00->width);
    CAD_EXPECT_EQ(reset_cell00->depth, expected_cell00->depth);
    CAD_EXPECT_EQ(reset_cell00->cell_type, expected_cell00->cell_type);
    CAD_EXPECT(reset_cell00->property_modes.geometry == cad::battery::PropertyMode::Generated);
}

CAD_TEST(reset_to_generated_restores_current_non_cyl_cell_dimensions)
{
    cad::CadEngine engine;
    cad::battery::BatteryCadConfig config;
    config.layout = makeSingleCellLayoutConfig(cad::battery::CellFormFactor::Pouch);
    config.layout.cell_width = 94.0f;
    config.layout.cell_depth = 7.5f;
    config.layout.cell_height = 102.0f;
    engine.setBatteryConfig(config);

    const auto* cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(cell00 != nullptr);

    const cad::core::EntityId cell00_id = cell00->id;
    cad::battery::CellPropertiesUpdate override_update;
    override_update.width = 126.0f;
    override_update.depth = 13.25f;
    override_update.height = 117.0f;
    CAD_EXPECT(engine.updateCellProperties(cell00_id, override_update));

    const auto* overridden_cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(overridden_cell00 != nullptr);
    CAD_EXPECT_EQ(overridden_cell00->width, *override_update.width);
    CAD_EXPECT_EQ(overridden_cell00->depth, *override_update.depth);
    CAD_EXPECT_EQ(overridden_cell00->height, *override_update.height);
    CAD_EXPECT(overridden_cell00->property_modes.geometry == cad::battery::PropertyMode::UserOverride);

    cad::battery::BatteryCadConfig updated = config;
    updated.layout.cell_width = 88.0f;
    updated.layout.cell_depth = 6.0f;
    updated.layout.cell_height = 96.0f;
    updated.layout.x_spacing += 10.0f;
    engine.setBatteryConfig(updated);

    const auto* preserved_cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(preserved_cell00 != nullptr);
    CAD_EXPECT_EQ(preserved_cell00->id, cell00_id);
    CAD_EXPECT_EQ(preserved_cell00->width, *override_update.width);
    CAD_EXPECT_EQ(preserved_cell00->depth, *override_update.depth);
    CAD_EXPECT_EQ(preserved_cell00->height, *override_update.height);
    CAD_EXPECT(preserved_cell00->property_modes.geometry == cad::battery::PropertyMode::UserOverride);

    const cad::core::CadDocument generated = buildGeneratedDocument(updated.layout);
    const auto* expected_cell00 = findCellByKey(generated, 0, 0);
    CAD_EXPECT(expected_cell00 != nullptr);
    CAD_EXPECT_NE(expected_cell00->width, preserved_cell00->width);
    CAD_EXPECT_NE(expected_cell00->depth, preserved_cell00->depth);
    CAD_EXPECT_NE(expected_cell00->height, preserved_cell00->height);

    CAD_EXPECT(engine.resetEntityGeometryToGenerated(cell00_id));

    const auto* reset_cell00 = findCellByKey(engine.document(), 0, 0);
    CAD_EXPECT(reset_cell00 != nullptr);
    CAD_EXPECT_EQ(reset_cell00->id, cell00_id);
    CAD_EXPECT_EQ(static_cast<int>(reset_cell00->form_factor), static_cast<int>(expected_cell00->form_factor));
    CAD_EXPECT_EQ(reset_cell00->width, expected_cell00->width);
    CAD_EXPECT_EQ(reset_cell00->depth, expected_cell00->depth);
    CAD_EXPECT_EQ(reset_cell00->height, expected_cell00->height);
    CAD_EXPECT_EQ(reset_cell00->cell_type, expected_cell00->cell_type);
    CAD_EXPECT(reset_cell00->property_modes.geometry == cad::battery::PropertyMode::Generated);
}
