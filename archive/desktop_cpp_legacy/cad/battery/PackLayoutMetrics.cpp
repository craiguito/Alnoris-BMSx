#include "PackLayoutMetrics.h"

#include <algorithm>
#include <numeric>

namespace cad::battery {
namespace {

float cylindricalDiameter(const PackLayoutConfig& config)
{
    return std::max(1.0f, config.cell_radius * 2.0f);
}

float resolvedCellWidth(const PackLayoutConfig& config)
{
    return config.cell_form_factor == CellFormFactor::Cylindrical
        ? cylindricalDiameter(config)
        : std::max(1.0f, config.cell_width);
}

float resolvedCellDepth(const PackLayoutConfig& config)
{
    return config.cell_form_factor == CellFormFactor::Cylindrical
        ? cylindricalDiameter(config)
        : std::max(1.0f, config.cell_depth);
}

float resolvedPitch(float requested_pitch, float footprint)
{
    return std::max(requested_pitch, footprint);
}

PackStackMetrics resolveStack(const PackLayoutConfig& config)
{
    PackStackMetrics stack;
    stack.tray_top_y = 0.0f;
    stack.tray_base_center_y = -config.module_tray_base_thickness * 0.5f;
    stack.tray_bottom_y = -config.module_tray_base_thickness;
    stack.support_pad_height = std::max(0.0f, config.cell_seating_offset);
    stack.support_pad_center_y = stack.support_pad_height * 0.5f;
    stack.cell_bottom_y = config.cell_seating_offset;
    stack.cell_center_y = stack.cell_bottom_y + config.cell_height * 0.5f;
    stack.cell_top_y = stack.cell_bottom_y + config.cell_height;
    stack.top_cap_top_y = stack.cell_top_y + config.top_cap_shoulder_height;
    stack.terminal_top_y = stack.top_cap_top_y + config.positive_terminal_height;
    stack.busbar_bottom_y = stack.terminal_top_y + config.busbar_terminal_clearance + config.busbar_support_offset;
    stack.busbar_center_y = stack.busbar_bottom_y + config.busbar_thickness * 0.5f;
    stack.busbar_top_y = stack.busbar_bottom_y + config.busbar_thickness;
    stack.cooling_plate_top_y = stack.tray_bottom_y - config.cooling_plate_offset_below_tray;
    stack.cooling_plate_center_y = stack.cooling_plate_top_y - config.cooling_channel_thickness * 0.5f;
    stack.cooling_plate_bottom_y = stack.cooling_plate_top_y - config.cooling_channel_thickness;
    stack.enclosure_floor_top_y = stack.cooling_plate_bottom_y - config.enclosure_floor_offset;
    stack.enclosure_floor_center_y = stack.enclosure_floor_top_y - config.enclosure_floor_thickness * 0.5f;
    stack.enclosure_floor_bottom_y = stack.enclosure_floor_top_y - config.enclosure_floor_thickness;
    stack.enclosure_side_wall_top_y = stack.busbar_top_y + std::max(6.0f, config.busbar_thickness * 4.0f);
    stack.bounds_center_y = (stack.enclosure_side_wall_top_y + stack.enclosure_floor_bottom_y) * 0.5f;
    stack.bounds_height = stack.enclosure_side_wall_top_y - stack.enclosure_floor_bottom_y;
    return stack;
}

std::vector<int> resolveModuleSeriesCounts(int series_count, int module_count)
{
    std::vector<int> counts;
    counts.reserve(static_cast<std::size_t>(module_count));
    const int base_series_per_module = series_count / module_count;
    const int remainder = series_count % module_count;
    for (int module_index = 0; module_index < module_count; ++module_index) {
        counts.push_back(base_series_per_module + (module_index < remainder ? 1 : 0));
    }
    return counts;
}

} // namespace

ResolvedPackLayout resolvePackLayout(const PackLayoutConfig& config)
{
    ResolvedPackLayout resolved;
    resolved.series_count = std::max(1, config.cells_in_series);
    resolved.parallel_count = std::max(1, config.cells_in_parallel);
    resolved.module_count = std::max(1, std::min(config.module_count, resolved.series_count));
    resolved.cylindrical = config.cell_form_factor == CellFormFactor::Cylindrical;
    resolved.cell_diameter = cylindricalDiameter(config);
    resolved.cell_width = resolvedCellWidth(config);
    resolved.cell_depth = resolvedCellDepth(config);

    resolved.pitch_x = resolvedPitch(config.x_spacing, resolved.cell_width);
    resolved.pitch_z = resolvedPitch(config.z_spacing, resolved.cell_depth);
    const float outer_row_center_z = (resolved.parallel_count - 1) * resolved.pitch_z * 0.5f;
    resolved.row_center_z = outer_row_center_z > 0.0f
        ? outer_row_center_z
        : std::max(resolved.cell_depth * 0.35f, config.busbar_width * 0.75f);
    resolved.cell_array_depth = (resolved.parallel_count - 1) * resolved.pitch_z + resolved.cell_depth;
    resolved.tray_depth = resolved.cell_array_depth + config.module_tray_margin_z * 2.0f;
    resolved.cooling_depth = resolved.tray_depth + config.cooling_plate_margin_z * 2.0f;
    resolved.boundary_depth = std::max(resolved.tray_depth, resolved.cooling_depth);
    resolved.group_width = std::max(resolved.pitch_x, resolved.cell_width + config.busbar_overlap_width);
    resolved.stack = resolveStack(config);

    const std::vector<int> module_series_counts = resolveModuleSeriesCounts(resolved.series_count, resolved.module_count);
    resolved.modules.reserve(module_series_counts.size());
    for (std::size_t index = 0; index < module_series_counts.size(); ++index) {
        const int module_series_count = module_series_counts[index];
        ModuleLayoutSlice slice;
        slice.module_index = static_cast<int>(index);
        slice.series_count = module_series_count;
        slice.cell_array_width = (module_series_count - 1) * resolved.pitch_x + resolved.cell_width;
        slice.tray_width = slice.cell_array_width + config.module_tray_margin_x * 2.0f;
        slice.cooling_width = slice.tray_width + config.cooling_plate_margin_x * 2.0f;
        slice.boundary_width = std::max(slice.tray_width, slice.cooling_width);
        slice.busbar_length = slice.cell_array_width + config.busbar_overlap_width * 2.0f;
        resolved.modules.push_back(slice);
    }

    const float modules_span_x = std::accumulate(
        resolved.modules.begin(),
        resolved.modules.end(),
        0.0f,
        [](float sum, const ModuleLayoutSlice& slice) {
            return sum + slice.boundary_width;
        }
    ) + std::max(0, resolved.module_count - 1) * config.module_gap_x;

    float running_x = -modules_span_x * 0.5f;
    int series_cursor = 0;
    for (ModuleLayoutSlice& slice : resolved.modules) {
        slice.series_start = series_cursor;
        slice.center_x = running_x + slice.boundary_width * 0.5f;
        running_x += slice.boundary_width + config.module_gap_x;
        series_cursor += slice.series_count;
    }

    resolved.pack_inner_width = modules_span_x + config.enclosure_clearance_x * 2.0f;
    resolved.pack_inner_depth = resolved.boundary_depth + config.enclosure_clearance_z * 2.0f;
    resolved.pack_outer_width = resolved.pack_inner_width + config.enclosure_wall_thickness * 2.0f;
    resolved.pack_outer_depth = resolved.pack_inner_depth + config.enclosure_wall_thickness * 2.0f;

    return resolved;
}

} // namespace cad::battery
