#include "BatteryGeometryGenerator.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace cad::geometry {
namespace {

using cad::math::Vec3;

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
        return cad::math::mix({0.14f, 0.45f, 0.94f}, {0.97f, 0.84f, 0.26f}, normalized / 0.5f);
    }
    return cad::math::mix({0.97f, 0.84f, 0.26f}, {0.95f, 0.32f, 0.25f}, (normalized - 0.5f) / 0.5f);
}

int quantizeColorComponent(float value)
{
    const float clamped = cad::math::clamp(value, 0.0f, 1.0f);
    return static_cast<int>(std::lround(clamped * 15.0f));
}

Vec3 overlayCellColor(const battery::BatteryVisualizationOverlay& overlay, const battery::CellEntity& cell)
{
    switch (overlay.active_metric) {
    case battery::BatteryVisualizationOverlay::Metric::CoreTemperature:
        if (const auto it = overlay.cell_core_temperature_c.find(cell.id); it != overlay.cell_core_temperature_c.end()) {
            return temperatureColor(it->second);
        }
        break;
    case battery::BatteryVisualizationOverlay::Metric::SurfaceTemperature:
        if (const auto it = overlay.cell_surface_temperature_c.find(cell.id); it != overlay.cell_surface_temperature_c.end()) {
            return temperatureColor(it->second);
        }
        break;
    case battery::BatteryVisualizationOverlay::Metric::Soc:
        if (const auto it = overlay.cell_soc.find(cell.id); it != overlay.cell_soc.end()) {
            return gradientColor(it->second, 0.0, 1.0);
        }
        break;
    case battery::BatteryVisualizationOverlay::Metric::Voltage:
        if (const auto it = overlay.cell_voltage_v.find(cell.id); it != overlay.cell_voltage_v.end()) {
            return gradientColor(it->second, 2.8, 4.25);
        }
        break;
    case battery::BatteryVisualizationOverlay::Metric::DiffusionStress:
        if (const auto it = overlay.cell_diffusion_stress.find(cell.id); it != overlay.cell_diffusion_stress.end()) {
            return gradientColor(it->second, 0.0, 1.0);
        }
        break;
    case battery::BatteryVisualizationOverlay::Metric::EffectiveResistance:
    default:
        if (const auto it = overlay.cell_effective_resistance_ohm.find(cell.id); it != overlay.cell_effective_resistance_ohm.end()) {
            return gradientColor(it->second, 0.0, 0.2);
        }
        break;
    }

    if (const auto it = overlay.cell_core_temperature_c.find(cell.id); it != overlay.cell_core_temperature_c.end()) {
        return temperatureColor(it->second);
    }
    return {0.73f, 0.70f, 0.66f};
}

void appendMesh(GeometryBuffer& geometry, const io::TriangleMesh& mesh, const Vec3& offset, const Vec3& color)
{
    for (std::size_t i = 0; i + 2 < mesh.vertices.size(); i += 3) {
        geometry.triangles.push_back({
            cad::math::add(mesh.vertices[i], offset),
            cad::math::add(mesh.vertices[i + 1], offset),
            cad::math::add(mesh.vertices[i + 2], offset),
            color,
            SurfaceLayer::Cell
        });
    }
}

void appendTranslatedGeometry(GeometryBuffer& target, const GeometryBuffer& local, const Vec3& offset)
{
    target.triangles.reserve(target.triangles.size() + local.triangles.size());
    for (const ColoredTriangle& triangle : local.triangles) {
        target.triangles.push_back({
            cad::math::add(triangle.a, offset),
            cad::math::add(triangle.b, offset),
            cad::math::add(triangle.c, offset),
            triangle.color,
            triangle.layer
        });
    }
    target.lines.reserve(target.lines.size() + local.lines.size());
    for (const ColoredLine& line : local.lines) {
        target.lines.push_back({
            cad::math::add(line.a, offset),
            cad::math::add(line.b, offset),
            line.color,
            line.layer
        });
    }
}

std::vector<const battery::CellEntity*> collectModuleCells(const core::CadDocument& document, core::EntityId module_id)
{
    std::vector<const battery::CellEntity*> cells;
    for (const core::EntityId entity_id : document.subtreeIds(module_id)) {
        if (const auto* cell = document.findCell(entity_id); cell != nullptr && cell->visible) {
            cells.push_back(cell);
        }
    }
    return cells;
}

struct ModuleGeometryContext
{
    std::vector<const battery::CellEntity*> cells;
    battery::BoundingBox cell_bounds{};
};

battery::BoundingBox boundsFromCells(const core::CadDocument& document, const std::vector<const battery::CellEntity*>& cells)
{
    if (cells.empty()) {
        return {};
    }

    Vec3 min_point{};
    Vec3 max_point{};
    bool initialized = false;
    for (const battery::CellEntity* cell : cells) {
        const battery::BoundingBox world_bounds = document.worldBounds(cell->id);
        const Vec3 half{world_bounds.size.x * 0.5f, world_bounds.size.y * 0.5f, world_bounds.size.z * 0.5f};
        const Vec3 local_min{
            world_bounds.center.x - half.x,
            world_bounds.center.y - half.y,
            world_bounds.center.z - half.z
        };
        const Vec3 local_max{
            world_bounds.center.x + half.x,
            world_bounds.center.y + half.y,
            world_bounds.center.z + half.z
        };
        if (!initialized) {
            min_point = local_min;
            max_point = local_max;
            initialized = true;
            continue;
        }
        min_point.x = std::min(min_point.x, local_min.x);
        min_point.y = std::min(min_point.y, local_min.y);
        min_point.z = std::min(min_point.z, local_min.z);
        max_point.x = std::max(max_point.x, local_max.x);
        max_point.y = std::max(max_point.y, local_max.y);
        max_point.z = std::max(max_point.z, local_max.z);
    }

    return {
        {(min_point.x + max_point.x) * 0.5f, (min_point.y + max_point.y) * 0.5f, (min_point.z + max_point.z) * 0.5f},
        {max_point.x - min_point.x, max_point.y - min_point.y, max_point.z - min_point.z}
    };
}

CylindricalCellProfile makeCellProfile(const battery::CellEntity& cell, const battery::PackLayoutConfig& layout)
{
    CylindricalCellProfile profile;
    profile.body_radius = cell.radius;
    profile.body_height = cell.height;
    profile.top_cap_shoulder_height = std::max(0.5f, layout.top_cap_shoulder_height);
    profile.top_cap_outer_radius = std::min(cell.radius * 0.98f, layout.top_cap_outer_diameter * 0.5f);
    profile.top_cap_inner_radius = std::min(profile.top_cap_outer_radius - 0.4f, layout.top_cap_inner_diameter * 0.5f);
    profile.terminal_radius = std::min(profile.top_cap_inner_radius * 0.92f, layout.positive_terminal_diameter * 0.5f);
    profile.terminal_height = std::max(0.6f, layout.positive_terminal_height);
    profile.insulator_outer_radius = std::min(profile.top_cap_outer_radius * 0.98f, layout.insulating_ring_outer_diameter * 0.5f);
    profile.insulator_inner_radius = std::min(profile.insulator_outer_radius - 0.6f, layout.insulating_ring_inner_diameter * 0.5f);
    profile.insulator_height = std::max(0.2f, layout.insulating_ring_height);
    profile.bottom_cap_height = std::max(0.2f, layout.bottom_cap_height);
    return profile;
}

void appendCoolingFeatures(GeometryBuffer& geometry, const battery::BoundingBox& bounds, const battery::PackLayoutConfig& config)
{
    const float inset_depth = std::min(config.cooling_channel_depth, config.cooling_channel_thickness * 0.35f);
    const float lane_width = std::max(10.0f, bounds.size.z * 0.12f);
    const float offset = bounds.size.z * 0.18f;
    const Vec3 feature_color{0.50f, 0.60f, 0.71f};

    appendBox(
        geometry,
        {bounds.center.x, bounds.center.y + bounds.size.y * 0.5f - inset_depth * 0.5f, bounds.center.z - offset},
        {bounds.size.x * 0.84f, inset_depth, lane_width},
        feature_color,
        SurfaceLayer::Support
    );
    appendBox(
        geometry,
        {bounds.center.x, bounds.center.y + bounds.size.y * 0.5f - inset_depth * 0.5f, bounds.center.z + offset},
        {bounds.size.x * 0.84f, inset_depth, lane_width},
        feature_color,
        SurfaceLayer::Support
    );
}

void appendBusbarGeometry(
    GeometryBuffer& geometry,
    const core::CadDocument& document,
    const battery::BusbarEntity& busbar,
    const battery::BoundingBox& bounds,
    const battery::BatteryModule* module,
    const ModuleGeometryContext& module_context,
    const battery::PackLayoutConfig& layout
)
{
    (void)document;
    (void)busbar;
    (void)module;
    (void)module_context;
    (void)layout;
    const Vec3 copper_color{0.72f, 0.48f, 0.24f};
    // Recovery path: keep each busbar entity as one coherent solid instead of
    // layering bridge tabs on top of the same span. The document/layout logic
    // still controls placement, but the rendered result stays mechanically legible.
    appendBox(geometry, bounds.center, bounds.size, copper_color, SurfaceLayer::Busbar);
}

void appendModuleTrayGeometry(
    GeometryBuffer& geometry,
    const core::CadDocument& document,
    const battery::ModuleBoundaryEntity& module,
    const ModuleGeometryContext& module_context,
    const battery::PackLayoutConfig& config
)
{
    (void)document;
    if (module_context.cells.empty()) {
        return;
    }

    const battery::BoundingBox cell_bounds = module_context.cell_bounds;
    const float tray_margin_x = config.module_tray_margin_x;
    const float tray_margin_z = config.module_tray_margin_z;
    const float tray_base_thickness = config.module_tray_base_thickness;
    const float tray_wall_thickness = config.module_tray_wall_thickness;
    const float tray_wall_height = config.module_tray_wall_height;
    const float seating_plane_y = cell_bounds.center.y - cell_bounds.size.y * 0.5f - config.cell_seating_offset;
    const Vec3 tray_color{0.40f, 0.45f, 0.52f};
    const Vec3 support_pad_color{0.32f, 0.36f, 0.41f};

    appendOpenTopTray(
        geometry,
        {
            cell_bounds.center.x,
            seating_plane_y,
            cell_bounds.center.z
        },
        {cell_bounds.size.x + tray_margin_x * 2.0f, cell_bounds.size.z + tray_margin_z * 2.0f, 0.0f},
        tray_base_thickness,
        tray_wall_thickness,
        tray_wall_height,
        tray_color,
        SurfaceLayer::Support
    );

    if (config.cell_seating_offset > 0.05f) {
        appendBox(
            geometry,
            {
                cell_bounds.center.x,
                seating_plane_y + config.cell_seating_offset * 0.5f,
                cell_bounds.center.z
            },
            {
                cell_bounds.size.x + 2.0f,
                config.cell_seating_offset,
                cell_bounds.size.z + 2.0f
            },
            support_pad_color,
            SurfaceLayer::Support
        );
    }
}

void appendEnclosureGeometry(
    GeometryBuffer& geometry,
    const battery::BoundingBox& bounds,
    const battery::PackLayoutConfig& config
)
{
    const Vec3 enclosure_color{0.79f, 0.81f, 0.84f};
    const float wall_height = bounds.size.y - config.enclosure_floor_thickness;
    appendOpenShell(
        geometry,
        bounds.center,
        bounds.size,
        config.enclosure_wall_thickness,
        config.enclosure_floor_thickness,
        wall_height,
        enclosure_color,
        SurfaceLayer::Background
    );
}

} // namespace

GeometryBuffer BatteryGeometryGenerator::buildCachedCylindricalCellGeometry(
    const CylindricalCellProfile& profile,
    int segments,
    const Vec3& body_color,
    const Vec3& cap_color,
    const Vec3& insulator_color
) const
{
    GeometryBuffer geometry;
    appendCylindricalCell(geometry, {0.0f, 0.0f, 0.0f}, profile, segments, body_color, cap_color, insulator_color, SurfaceLayer::Cell);
    return geometry;
}

const GeometryBuffer& BatteryGeometryGenerator::cachedCylindricalCellGeometry(
    const CylindricalCellProfile& profile,
    int segments,
    const Vec3& body_color,
    const Vec3& cap_color,
    const Vec3& insulator_color
) const
{
    std::ostringstream key_stream;
    key_stream
        << std::lround(profile.body_radius * 100.0f) << ':'
        << std::lround(profile.body_height * 100.0f) << ':'
        << std::lround(profile.top_cap_shoulder_height * 100.0f) << ':'
        << std::lround(profile.top_cap_outer_radius * 100.0f) << ':'
        << std::lround(profile.top_cap_inner_radius * 100.0f) << ':'
        << std::lround(profile.terminal_radius * 100.0f) << ':'
        << std::lround(profile.terminal_height * 100.0f) << ':'
        << std::lround(profile.insulator_outer_radius * 100.0f) << ':'
        << std::lround(profile.insulator_inner_radius * 100.0f) << ':'
        << std::lround(profile.insulator_height * 100.0f) << ':'
        << std::lround(profile.bottom_cap_height * 100.0f) << ':'
        << quantizeColorComponent(body_color.x) << ','
        << quantizeColorComponent(body_color.y) << ','
        << quantizeColorComponent(body_color.z) << ':'
        << quantizeColorComponent(cap_color.x) << ','
        << quantizeColorComponent(cap_color.y) << ','
        << quantizeColorComponent(cap_color.z) << ':'
        << quantizeColorComponent(insulator_color.x) << ','
        << quantizeColorComponent(insulator_color.y) << ','
        << quantizeColorComponent(insulator_color.z) << ':'
        << segments;
    const std::string key = key_stream.str();

    const auto existing = m_cylindricalCellCache.find(key);
    if (existing != m_cylindricalCellCache.end()) {
        m_cacheStats.cylindrical_cell_hits += 1;
        return existing->second;
    }

    auto inserted = m_cylindricalCellCache.emplace(
        key,
        buildCachedCylindricalCellGeometry(profile, segments, body_color, cap_color, insulator_color)
    );
    m_cacheStats.cylindrical_cell_misses += 1;
    return inserted.first->second;
}

GeometryBuffer BatteryGeometryGenerator::buildVisualGeometry(
    const core::CadDocument& document,
    const battery::BatteryVisualizationOverlay& overlay,
    const io::TriangleMesh* cell_mesh
) const
{
    GeometryBuffer geometry;
    const battery::PackLayoutConfig& layout = document.metadata().layout_config;
    const int cell_count = static_cast<int>(document.cells().size());
    const int cylindrical_segments = cell_count > 300 ? 8 : (cell_count > 150 ? 10 : 14);
    const bool large_pack_mode = cell_count > 200;
    std::unordered_map<core::EntityId, ModuleGeometryContext, core::EntityIdHash> module_contexts;
    module_contexts.reserve(document.modules().size());

    for (const battery::ModuleBoundaryEntity& module : document.modules()) {
        ModuleGeometryContext context;
        context.cells = collectModuleCells(document, module.id);
        context.cell_bounds = boundsFromCells(document, context.cells);
        module_contexts.emplace(module.id, std::move(context));
    }

    for (const battery::ModuleBoundaryEntity& module : document.modules()) {
        if (!module.visible) {
            continue;
        }
        const auto context_it = module_contexts.find(module.id);
        if (context_it != module_contexts.end()) {
            appendModuleTrayGeometry(geometry, document, module, context_it->second, layout);
        }
    }

    for (const battery::CoolingPlateEntity& plate : document.coolingPlates()) {
        if (!plate.visible) {
            continue;
        }
        const battery::BoundingBox bounds = document.worldBounds(plate.id);
        appendBox(
            geometry,
            bounds.center,
            bounds.size,
            {0.42f, 0.54f, 0.66f},
            SurfaceLayer::Support
        );
        if (!large_pack_mode) {
            appendCoolingFeatures(geometry, bounds, layout);
        }
    }

    for (const battery::BusbarEntity& busbar : document.busbars()) {
        if (!busbar.visible) {
            continue;
        }
        const auto* module = document.findModuleBoundary(busbar.parent_id);
        if (module == nullptr) {
            continue;
        }
        const auto context_it = module_contexts.find(module->id);
        if (context_it == module_contexts.end()) {
            continue;
        }
        appendBusbarGeometry(geometry, document, busbar, document.worldBounds(busbar.id), module, context_it->second, layout);
    }

    for (const battery::PackEnclosureEntity& enclosure : document.packEnclosures()) {
        if (!enclosure.visible) {
            continue;
        }
        if (!large_pack_mode) {
            appendEnclosureGeometry(geometry, document.worldBounds(enclosure.id), layout);
        }
    }

    for (const battery::CellEntity& cell : document.cells()) {
        if (!cell.visible) {
            continue;
        }

        const Vec3 world_position = document.worldPosition(cell.id);
        const Vec3 body_color = overlayCellColor(overlay, cell);
        if (cell_mesh != nullptr && !cell_mesh->vertices.empty()) {
            appendMesh(geometry, *cell_mesh, world_position, body_color);
            continue;
        }

        if (cell.form_factor == battery::CellFormFactor::Cylindrical) {
            const GeometryBuffer& cell_geometry = cachedCylindricalCellGeometry(
                makeCellProfile(cell, layout),
                cylindrical_segments,
                body_color,
                {0.86f, 0.88f, 0.91f},
                {0.95f, 0.92f, 0.77f}
            );
            appendTranslatedGeometry(geometry, cell_geometry, world_position);
        } else {
            appendBox(geometry, world_position, {cell.width, cell.height, cell.depth}, body_color, SurfaceLayer::Cell);
        }
    }

    return geometry;
}

} // namespace cad::geometry
