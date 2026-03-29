#pragma once

#include "BatteryConfig.h"

#include <vector>

namespace cad::battery {

struct ModuleLayoutSlice
{
    int module_index = 0;
    int series_start = 0;
    int series_count = 0;
    float center_x = 0.0f;
    float cell_array_width = 0.0f;
    float tray_width = 0.0f;
    float cooling_width = 0.0f;
    float boundary_width = 0.0f;
    float busbar_length = 0.0f;
};

struct PackStackMetrics
{
    float tray_top_y = 0.0f;
    float tray_base_center_y = 0.0f;
    float tray_bottom_y = 0.0f;
    float support_pad_center_y = 0.0f;
    float support_pad_height = 0.0f;
    float cell_bottom_y = 0.0f;
    float cell_center_y = 0.0f;
    float cell_top_y = 0.0f;
    float top_cap_top_y = 0.0f;
    float terminal_top_y = 0.0f;
    float busbar_bottom_y = 0.0f;
    float busbar_center_y = 0.0f;
    float busbar_top_y = 0.0f;
    float cooling_plate_top_y = 0.0f;
    float cooling_plate_center_y = 0.0f;
    float cooling_plate_bottom_y = 0.0f;
    float enclosure_floor_top_y = 0.0f;
    float enclosure_floor_center_y = 0.0f;
    float enclosure_floor_bottom_y = 0.0f;
    float enclosure_side_wall_top_y = 0.0f;
    float bounds_center_y = 0.0f;
    float bounds_height = 0.0f;
};

struct ResolvedPackLayout
{
    int series_count = 1;
    int parallel_count = 1;
    int module_count = 1;
    bool cylindrical = true;
    float cell_diameter = 21.0f;
    float cell_width = 21.0f;
    float cell_depth = 21.0f;
    float pitch_x = 23.0f;
    float pitch_z = 23.0f;
    float row_center_z = 0.0f;
    float cell_array_depth = 21.0f;
    float tray_depth = 41.0f;
    float cooling_depth = 53.0f;
    float boundary_depth = 53.0f;
    float pack_outer_width = 0.0f;
    float pack_outer_depth = 0.0f;
    float pack_inner_width = 0.0f;
    float pack_inner_depth = 0.0f;
    float group_width = 23.0f;
    PackStackMetrics stack;
    std::vector<ModuleLayoutSlice> modules;
};

[[nodiscard]] ResolvedPackLayout resolvePackLayout(const PackLayoutConfig& config);

} // namespace cad::battery
