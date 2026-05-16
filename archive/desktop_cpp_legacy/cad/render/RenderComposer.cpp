#include "RenderComposer.h"

#include "../geometry/BatteryGeometryGenerator.h"

#include <algorithm>

namespace cad::render {
namespace {

using cad::math::Mat4;
using cad::math::Vec3;
using cad::math::Vec4;

void appendWireBox(std::vector<RenderVertex>& lines, const Vec3& center, const Vec3& size, const Vec3& color);
void appendCornerBox(
    std::vector<RenderVertex>& lines,
    const Vec3& center,
    const Vec3& size,
    const Vec3& color,
    float corner_fraction = 0.22f
);

Vec3 temperatureColor(double temp_c)
{
    const float normalized = cad::math::clamp(static_cast<float>((temp_c - 20.0) / 28.0), 0.0f, 1.0f);
    return cad::math::mix({0.28f, 0.73f, 1.0f}, {1.0f, 0.42f, 0.25f}, normalized);
}

Vec3 gradientColor(double value, double min_value, double max_value)
{
    if (max_value <= min_value) {
        return {0.70f, 0.74f, 0.79f};
    }

    const float normalized = cad::math::clamp(static_cast<float>((value - min_value) / (max_value - min_value)), 0.0f, 1.0f);
    if (normalized < 0.5f) {
        const float local = normalized / 0.5f;
        return cad::math::mix({0.14f, 0.45f, 0.94f}, {0.97f, 0.84f, 0.26f}, local);
    }
    const float local = (normalized - 0.5f) / 0.5f;
    return cad::math::mix({0.97f, 0.84f, 0.26f}, {0.95f, 0.32f, 0.25f}, local);
}

Vec3 overlayCellColor(const battery::BatteryVisualizationOverlay& overlay, const battery::CellEntity& cell)
{
    switch (overlay.active_metric) {
    case battery::BatteryVisualizationOverlay::Metric::CoreTemperature: {
        const auto it = overlay.cell_core_temperature_c.find(cell.id);
        if (it != overlay.cell_core_temperature_c.end()) {
            return temperatureColor(it->second);
        }
        break;
    }
    case battery::BatteryVisualizationOverlay::Metric::SurfaceTemperature: {
        const auto it = overlay.cell_surface_temperature_c.find(cell.id);
        if (it != overlay.cell_surface_temperature_c.end()) {
            return temperatureColor(it->second);
        }
        break;
    }
    case battery::BatteryVisualizationOverlay::Metric::Soc: {
        const auto it = overlay.cell_soc.find(cell.id);
        if (it != overlay.cell_soc.end()) {
            return gradientColor(it->second, 0.0, 1.0);
        }
        break;
    }
    case battery::BatteryVisualizationOverlay::Metric::Voltage: {
        const auto it = overlay.cell_voltage_v.find(cell.id);
        if (it != overlay.cell_voltage_v.end()) {
            return gradientColor(it->second, 2.8, 4.25);
        }
        break;
    }
    case battery::BatteryVisualizationOverlay::Metric::DiffusionStress: {
        const auto it = overlay.cell_diffusion_stress.find(cell.id);
        if (it != overlay.cell_diffusion_stress.end()) {
            return gradientColor(it->second, 0.0, 1.0);
        }
        break;
    }
    case battery::BatteryVisualizationOverlay::Metric::EffectiveResistance:
    default: {
        const auto it = overlay.cell_effective_resistance_ohm.find(cell.id);
        if (it != overlay.cell_effective_resistance_ohm.end()) {
            return gradientColor(it->second, 0.0, 0.2);
        }
        break;
    }
    }

    if (const auto it = overlay.cell_core_temperature_c.find(cell.id); it != overlay.cell_core_temperature_c.end()) {
        return temperatureColor(it->second);
    }
    return {0.73f, 0.70f, 0.66f};
}

void appendTriangle(std::vector<RenderVertex>& vertices, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& color)
{
    vertices.push_back({a, color});
    vertices.push_back({b, color});
    vertices.push_back({c, color});
}

void appendQuad(std::vector<RenderVertex>& vertices, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& color)
{
    appendTriangle(vertices, a, b, c, color);
    appendTriangle(vertices, a, c, d, color);
}

void appendBox(std::vector<RenderVertex>& vertices, const Vec3& center, const Vec3& size, const Vec3& color)
{
    const Vec3 half{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};
    const Vec3 p000{center.x - half.x, center.y - half.y, center.z - half.z};
    const Vec3 p001{center.x - half.x, center.y - half.y, center.z + half.z};
    const Vec3 p010{center.x - half.x, center.y + half.y, center.z - half.z};
    const Vec3 p011{center.x - half.x, center.y + half.y, center.z + half.z};
    const Vec3 p100{center.x + half.x, center.y - half.y, center.z - half.z};
    const Vec3 p101{center.x + half.x, center.y - half.y, center.z + half.z};
    const Vec3 p110{center.x + half.x, center.y + half.y, center.z - half.z};
    const Vec3 p111{center.x + half.x, center.y + half.y, center.z + half.z};

    appendQuad(vertices, p000, p100, p110, p010, color);
    appendQuad(vertices, p101, p001, p011, p111, color);
    appendQuad(vertices, p001, p000, p010, p011, color);
    appendQuad(vertices, p100, p101, p111, p110, color);
    appendQuad(vertices, p010, p110, p111, p011, cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.08f));
    appendQuad(vertices, p001, p101, p100, p000, cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.14f));
}

void appendCylinder(std::vector<RenderVertex>& vertices, const Vec3& center, float radius, float height, int segments, const Vec3& color)
{
    const float half_height = height * 0.5f;
    const Vec3 top_center{center.x, center.y + half_height, center.z};
    const Vec3 bottom_center{center.x, center.y - half_height, center.z};

    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / segments) * 2.0f * cad::math::kPi;
        const float a1 = (static_cast<float>(i + 1) / segments) * 2.0f * cad::math::kPi;
        const Vec3 p0{radius * std::cos(a0), 0.0f, radius * std::sin(a0)};
        const Vec3 p1{radius * std::cos(a1), 0.0f, radius * std::sin(a1)};

        const Vec3 side = cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.10f + 0.18f * std::max(0.0f, std::cos(a0)));
        appendQuad(vertices, cad::math::add(top_center, p0), cad::math::add(top_center, p1), cad::math::add(bottom_center, p1), cad::math::add(bottom_center, p0), side);
        appendTriangle(vertices, top_center, cad::math::add(top_center, p1), cad::math::add(top_center, p0), cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.24f));
        appendTriangle(vertices, bottom_center, cad::math::add(bottom_center, p0), cad::math::add(bottom_center, p1), cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.18f));
    }
}

void appendMesh(std::vector<RenderVertex>& vertices, const io::TriangleMesh& mesh, const Vec3& offset, const Vec3& color)
{
    for (const Vec3& point : mesh.vertices) {
        vertices.push_back({cad::math::add(point, offset), color});
    }
}

float safeAxisScale(float source_extent, float target_extent)
{
    if (source_extent > 0.0001f) {
        return target_extent / source_extent;
    }
    return 1.0f;
}

Vec3 meshScaleForCell(const io::TriangleMesh& mesh, const battery::CellEntity& cell)
{
    const Vec3 target_size = cell.localBounds().size;
    return {
        safeAxisScale(mesh.source_size.x, target_size.x),
        safeAxisScale(mesh.source_size.y, target_size.y),
        safeAxisScale(mesh.source_size.z, target_size.z)
    };
}

Vec3 transformMeshPoint(const io::TriangleMesh& mesh, const Vec3& scale, const Vec3& point, const Vec3& offset)
{
    const Vec3 centered = cad::math::sub(point, mesh.source_center);
    return {
        offset.x + centered.x * scale.x,
        offset.y + centered.y * scale.y,
        offset.z + centered.z * scale.z
    };
}

void appendMeshBoundsWireframe(
    std::vector<RenderVertex>& lines,
    const io::TriangleMesh& mesh,
    const battery::CellEntity& cell,
    const Vec3& offset,
    const Vec3& color)
{
    if (mesh.vertices.empty()) {
        return;
    }

    const Vec3 scale = meshScaleForCell(mesh, cell);
    Vec3 min_point = transformMeshPoint(mesh, scale, mesh.vertices.front(), offset);
    Vec3 max_point = min_point;
    for (const Vec3& point : mesh.vertices) {
        const Vec3 transformed = transformMeshPoint(mesh, scale, point, offset);
        min_point.x = std::min(min_point.x, transformed.x);
        min_point.y = std::min(min_point.y, transformed.y);
        min_point.z = std::min(min_point.z, transformed.z);
        max_point.x = std::max(max_point.x, transformed.x);
        max_point.y = std::max(max_point.y, transformed.y);
        max_point.z = std::max(max_point.z, transformed.z);
    }

    const Vec3 center{
        (min_point.x + max_point.x) * 0.5f,
        (min_point.y + max_point.y) * 0.5f,
        (min_point.z + max_point.z) * 0.5f
    };
    const Vec3 size{
        (max_point.x - min_point.x),
        (max_point.y - min_point.y),
        (max_point.z - min_point.z)
    };
    appendWireBox(lines, center, size, color);
}

struct ProjectionResult
{
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    bool valid = false;
};

ProjectionResult projectPoint(const Mat4& mvp, const Vec3& p, int width, int height)
{
    const Vec4 clip = cad::math::multiply(mvp, Vec4{p.x, p.y, p.z, 1.0f});
    if (clip.w <= 0.0001f) {
        return {};
    }

    const float ndc_x = clip.x / clip.w;
    const float ndc_y = clip.y / clip.w;
    const float ndc_z = clip.z / clip.w;
    return {
        (ndc_x * 0.5f + 0.5f) * width,
        (1.0f - (ndc_y * 0.5f + 0.5f)) * height,
        ndc_z,
        true
    };
}

void appendBoxPickable(
    std::vector<ScreenPickable>& pickables,
    core::EntityId entity_id,
    int selection_priority,
    const Mat4& mvp,
    const Vec3& center,
    const Vec3& size,
    int width,
    int height
)
{
    const Vec3 half{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};
    const Vec3 corners[] = {
        {center.x - half.x, center.y - half.y, center.z - half.z},
        {center.x - half.x, center.y - half.y, center.z + half.z},
        {center.x - half.x, center.y + half.y, center.z - half.z},
        {center.x - half.x, center.y + half.y, center.z + half.z},
        {center.x + half.x, center.y - half.y, center.z - half.z},
        {center.x + half.x, center.y - half.y, center.z + half.z},
        {center.x + half.x, center.y + half.y, center.z - half.z},
        {center.x + half.x, center.y + half.y, center.z + half.z}
    };

    float min_x = static_cast<float>(width);
    float min_y = static_cast<float>(height);
    float max_x = 0.0f;
    float max_y = 0.0f;
    float depth_accumulator = 0.0f;
    int valid_count = 0;

    for (const Vec3& corner : corners) {
        const ProjectionResult projected = projectPoint(mvp, corner, width, height);
        if (!projected.valid) {
            continue;
        }
        min_x = std::min(min_x, projected.x);
        min_y = std::min(min_y, projected.y);
        max_x = std::max(max_x, projected.x);
        max_y = std::max(max_y, projected.y);
        depth_accumulator += projected.depth;
        ++valid_count;
    }

    if (valid_count < 2) {
        return;
    }

    pickables.push_back({
        entity_id,
        ScreenPickable::Shape::Rectangle,
        selection_priority,
        (min_x + max_x) * 0.5f,
        (min_y + max_y) * 0.5f,
        std::max(5.0f, (max_x - min_x) * 0.5f),
        std::max(5.0f, (max_y - min_y) * 0.5f),
        depth_accumulator / static_cast<float>(valid_count)
    });
}

bool pickableSortsBefore(const ScreenPickable& a, const ScreenPickable& b)
{
    if (a.depth != b.depth) {
        return a.depth < b.depth;
    }
    if (a.selection_priority != b.selection_priority) {
        return a.selection_priority > b.selection_priority;
    }
    return a.entity_id.value < b.entity_id.value;
}

void appendWireBox(std::vector<RenderVertex>& lines, const Vec3& center, const Vec3& size, const Vec3& color)
{
    const Vec3 half{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};
    const Vec3 p000{center.x - half.x, center.y - half.y, center.z - half.z};
    const Vec3 p001{center.x - half.x, center.y - half.y, center.z + half.z};
    const Vec3 p010{center.x - half.x, center.y + half.y, center.z - half.z};
    const Vec3 p011{center.x - half.x, center.y + half.y, center.z + half.z};
    const Vec3 p100{center.x + half.x, center.y - half.y, center.z - half.z};
    const Vec3 p101{center.x + half.x, center.y - half.y, center.z + half.z};
    const Vec3 p110{center.x + half.x, center.y + half.y, center.z - half.z};
    const Vec3 p111{center.x + half.x, center.y + half.y, center.z + half.z};

    const Vec3 pairs[] = {
        p000, p001, p000, p010, p000, p100,
        p111, p110, p111, p101, p111, p011,
        p001, p011, p001, p101,
        p010, p011, p010, p110,
        p100, p101, p100, p110
    };
    for (std::size_t i = 0; i < std::size(pairs); i += 2) {
        lines.push_back({pairs[i], color});
        lines.push_back({pairs[i + 1], color});
    }
}

void appendCornerBox(
    std::vector<RenderVertex>& lines,
    const Vec3& center,
    const Vec3& size,
    const Vec3& color,
    float corner_fraction
)
{
    const Vec3 half{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};
    const Vec3 corner{
        std::max(6.0f, size.x * corner_fraction),
        std::max(6.0f, size.y * corner_fraction),
        std::max(6.0f, size.z * corner_fraction)
    };

    const int signs[2] = {-1, 1};
    for (const int sx : signs) {
        for (const int sy : signs) {
            for (const int sz : signs) {
                const Vec3 p{
                    center.x + half.x * static_cast<float>(sx),
                    center.y + half.y * static_cast<float>(sy),
                    center.z + half.z * static_cast<float>(sz)
                };

                lines.push_back({p, color});
                lines.push_back({{p.x - corner.x * static_cast<float>(sx), p.y, p.z}, color});
                lines.push_back({p, color});
                lines.push_back({{p.x, p.y - corner.y * static_cast<float>(sy), p.z}, color});
                lines.push_back({p, color});
                lines.push_back({{p.x, p.y, p.z - corner.z * static_cast<float>(sz)}, color});
            }
        }
    }
}

void appendWireCylinder(std::vector<RenderVertex>& lines, const Vec3& center, float radius, float height, int segments, const Vec3& color)
{
    const float half_height = height * 0.5f;
    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / segments) * 2.0f * cad::math::kPi;
        const float a1 = (static_cast<float>(i + 1) / segments) * 2.0f * cad::math::kPi;
        const Vec3 top0{center.x + radius * std::cos(a0), center.y + half_height, center.z + radius * std::sin(a0)};
        const Vec3 top1{center.x + radius * std::cos(a1), center.y + half_height, center.z + radius * std::sin(a1)};
        const Vec3 bottom0{center.x + radius * std::cos(a0), center.y - half_height, center.z + radius * std::sin(a0)};
        const Vec3 bottom1{center.x + radius * std::cos(a1), center.y - half_height, center.z + radius * std::sin(a1)};

        lines.push_back({top0, color});
        lines.push_back({top1, color});
        lines.push_back({bottom0, color});
        lines.push_back({bottom1, color});

        if ((i % std::max(1, segments / 6)) == 0) {
            lines.push_back({top0, color});
            lines.push_back({bottom0, color});
        }
    }
}

} // namespace

RenderPacket RenderComposer::compose(
    const core::CadDocument& document,
    const battery::BatteryVisualizationOverlay& overlay,
    const geometry::GeometryBuffer& scene_geometry,
    const camera::Camera& camera,
    const io::TriangleMesh* cell_mesh,
    int viewport_width,
    int viewport_height
) const
{
    (void)scene_geometry;
    (void)cell_mesh;
    RenderPacket packet;
    packet.lines.reserve(128);
    packet.selection_overlay_lines.reserve(256);
    packet.pickables.reserve(
        document.cells().size()
        + document.busbars().size()
        + document.coolingPlates().size()
        + document.moduleBoundaries().size()
        + document.packEnclosures().size()
        + document.cellGroups().size()
        + document.packs().size()
    );

    const Mat4 projection = camera.projectionMatrix(static_cast<float>(std::max(1, viewport_width)) / static_cast<float>(std::max(1, viewport_height)));
    const Mat4 view = camera.viewMatrix();
    const Mat4 mvp = cad::math::multiply(projection, view);
    packet.mvp = mvp.m;

    const std::vector<core::EntityId> selected_subtree = document.subtreeIds(document.selection().primary);
    const auto isSelectedOrDescendant = [&selected_subtree](core::EntityId id) {
        return std::find(selected_subtree.begin(), selected_subtree.end(), id) != selected_subtree.end();
    };
    const auto isEffectivelyVisible = [&document](core::EntityId id) {
        return document.isEffectivelyVisible(id);
    };

    const Vec3 grid_color{0.86f, 0.88f, 0.91f};
    const Vec3 grid_axis_color{0.79f, 0.82f, 0.86f};
    const Vec3 pack_structure_color{0.74f, 0.77f, 0.80f};
    const Vec3 module_structure_color{0.67f, 0.72f, 0.78f};
    const Vec3 group_structure_color{0.69f, 0.73f, 0.77f};
    const Vec3 selected_overlay_color{0.11f, 0.45f, 0.98f};
    const Vec3 cooling_color{0.56f, 0.66f, 0.76f};
    const Vec3 busbar_color{0.72f, 0.48f, 0.24f};
    const Vec3 enclosure_color{0.80f, 0.82f, 0.84f};
    const int grid_extent = 10;
    const float grid_step = 70.0f;
    for (int i = -grid_extent; i <= grid_extent; ++i) {
        if ((i % 3) != 0) {
            continue;
        }
        const float offset = i * grid_step;
        const Vec3 color = (i == 0) ? grid_axis_color : grid_color;
        packet.lines.push_back({{offset, -120.0f, -grid_extent * grid_step}, color, 0});
        packet.lines.push_back({{offset, -120.0f, grid_extent * grid_step}, color, 0});
        packet.lines.push_back({{-grid_extent * grid_step, -120.0f, offset}, color, 0});
        packet.lines.push_back({{grid_extent * grid_step, -120.0f, offset}, color, 0});
    }

    for (const battery::BatteryPackEntity& pack : document.packs()) {
        if (!isEffectivelyVisible(pack.id)) {
            continue;
        }
        const battery::BoundingBox bounds = document.worldBounds(pack.id);
        appendBoxPickable(packet.pickables, pack.id, 80, mvp, bounds.center, bounds.size, viewport_width, viewport_height);
        if (isSelectedOrDescendant(pack.id)) {
            appendCornerBox(packet.selection_overlay_lines, bounds.center, bounds.size, pack_structure_color, 0.16f);
        }
    }
    for (const battery::ModuleBoundaryEntity& module : document.modules()) {
        if (!isEffectivelyVisible(module.id)) {
            continue;
        }
        const battery::BoundingBox bounds = document.worldBounds(module.id);
        appendBoxPickable(packet.pickables, module.id, 160, mvp, bounds.center, bounds.size, viewport_width, viewport_height);
        if (isSelectedOrDescendant(module.id)) {
            appendCornerBox(packet.selection_overlay_lines, bounds.center, bounds.size, module_structure_color, 0.16f);
        }
    }
    for (const battery::CellGroupEntity& group : document.cellGroups()) {
        if (!isEffectivelyVisible(group.id)) {
            continue;
        }
        const battery::BoundingBox bounds = document.worldBounds(group.id);
        appendBoxPickable(packet.pickables, group.id, 240, mvp, bounds.center, bounds.size, viewport_width, viewport_height);
        if (isSelectedOrDescendant(group.id)) {
            appendCornerBox(packet.selection_overlay_lines, bounds.center, bounds.size, group_structure_color, 0.14f);
        }
    }

    for (const battery::CoolingPlateEntity& plate : document.coolingPlates()) {
        if (!isEffectivelyVisible(plate.id)) {
            continue;
        }
        const battery::BoundingBox bounds = document.worldBounds(plate.id);
        appendBoxPickable(packet.pickables, plate.id, 300, mvp, bounds.center, bounds.size, viewport_width, viewport_height);
        if (isSelectedOrDescendant(plate.id)) {
            appendWireBox(packet.selection_overlay_lines, bounds.center, bounds.size, selected_overlay_color);
        }
    }
    for (const battery::BusbarEntity& busbar : document.busbars()) {
        if (!isEffectivelyVisible(busbar.id)) {
            continue;
        }
        const battery::BoundingBox bounds = document.worldBounds(busbar.id);
        appendBoxPickable(packet.pickables, busbar.id, 400, mvp, bounds.center, bounds.size, viewport_width, viewport_height);
        if (isSelectedOrDescendant(busbar.id)) {
            appendWireBox(packet.selection_overlay_lines, bounds.center, bounds.size, selected_overlay_color);
        }
    }
    for (const battery::PackEnclosureEntity& enclosure : document.packEnclosures()) {
        if (!isEffectivelyVisible(enclosure.id)) {
            continue;
        }
        const battery::BoundingBox bounds = document.worldBounds(enclosure.id);
        appendBoxPickable(packet.pickables, enclosure.id, 120, mvp, bounds.center, bounds.size, viewport_width, viewport_height);
        if (isSelectedOrDescendant(enclosure.id)) {
            appendCornerBox(packet.selection_overlay_lines, bounds.center, bounds.size, enclosure_color, 0.12f);
        }
    }

    for (const battery::CellEntity& cell : document.cells()) {
        if (!isEffectivelyVisible(cell.id)) {
            continue;
        }
        const Vec3 world_position = document.worldPosition(cell.id);
        if (cell_mesh != nullptr && !cell_mesh->vertices.empty() && isSelectedOrDescendant(cell.id)) {
            appendMeshBoundsWireframe(packet.selection_overlay_lines, *cell_mesh, cell, world_position, selected_overlay_color);
        } else if (cell.form_factor == battery::CellFormFactor::Cylindrical) {
            if (isSelectedOrDescendant(cell.id)) {
                appendWireCylinder(packet.selection_overlay_lines, world_position, cell.radius + 1.6f, cell.height + 3.0f, 28, selected_overlay_color);
            }
        } else if (isSelectedOrDescendant(cell.id)) {
            appendWireBox(packet.selection_overlay_lines, world_position, {cell.width + 2.0f, cell.height + 2.0f, cell.depth + 2.0f}, selected_overlay_color);
        }

        if (cell.form_factor == battery::CellFormFactor::Cylindrical) {
            const ProjectionResult center = projectPoint(mvp, world_position, viewport_width, viewport_height);
            const ProjectionResult edge = projectPoint(
                mvp,
                {world_position.x + (cell.radius + 4.0f), world_position.y, world_position.z},
                viewport_width,
                viewport_height
            );
            if (center.valid && edge.valid) {
                packet.pickables.push_back({
                    cell.id,
                    ScreenPickable::Shape::Circle,
                    500,
                    center.x,
                    center.y,
                    std::max(6.0f, std::abs(edge.x - center.x)),
                    std::max(6.0f, std::abs(edge.x - center.x)),
                    center.depth
                });
            }
        } else {
            appendBoxPickable(packet.pickables, cell.id, 500, mvp, world_position, {cell.width, cell.height, cell.depth}, viewport_width, viewport_height);
        }
    }

    std::sort(packet.pickables.begin(), packet.pickables.end(), pickableSortsBefore);

    packet.lines.push_back({{-180.0f, 0.0f, 0.0f}, {0.79f, 0.72f, 0.72f}, 0});
    packet.lines.push_back({{180.0f, 0.0f, 0.0f}, {0.79f, 0.72f, 0.72f}, 0});
    packet.lines.push_back({{0.0f, -90.0f, 0.0f}, {0.71f, 0.79f, 0.73f}, 0});
    packet.lines.push_back({{0.0f, 180.0f, 0.0f}, {0.71f, 0.79f, 0.73f}, 0});
    packet.lines.push_back({{0.0f, 0.0f, -180.0f}, {0.71f, 0.75f, 0.82f}, 0});
    packet.lines.push_back({{0.0f, 0.0f, 180.0f}, {0.71f, 0.75f, 0.82f}, 0});

    return packet;
}

} // namespace cad::render
