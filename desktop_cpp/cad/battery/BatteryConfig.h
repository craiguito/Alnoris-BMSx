#pragma once

#include <string>

namespace cad::battery {

enum class CellFormFactor
{
    Cylindrical,
    Prismatic,
    Pouch
};

struct PackLayoutConfig
{
    std::string preset_name = "Custom";
    int cells_in_series = 4;
    int cells_in_parallel = 2;
    int module_count = 1;
    CellFormFactor cell_form_factor = CellFormFactor::Cylindrical;
    float cell_radius = 10.5f;
    float cell_height = 70.0f;
    float cell_width = 21.0f;
    float cell_depth = 21.0f;
    float top_cap_outer_diameter = 18.5f;
    float top_cap_inner_diameter = 14.5f;
    float top_cap_shoulder_height = 1.0f;
    float positive_terminal_diameter = 8.0f;
    float positive_terminal_height = 1.4f;
    float insulating_ring_outer_diameter = 16.5f;
    float insulating_ring_inner_diameter = 9.0f;
    float insulating_ring_height = 0.5f;
    float bottom_cap_height = 0.8f;
    float x_spacing = 23.0f;
    float z_spacing = 23.0f;
    float module_gap_x = 28.0f;
    float busbar_thickness = 1.5f;
    float busbar_width = 8.0f;
    float busbar_terminal_clearance = 1.5f;
    float busbar_support_offset = 0.8f;
    float busbar_overlap_width = 7.0f;
    float busbar_tab_width = 7.0f;
    float busbar_tab_depth = 8.0f;
    float cooling_channel_thickness = 4.0f;
    float cooling_channel_depth = 0.8f;
    float cooling_plate_margin_x = 6.0f;
    float cooling_plate_margin_z = 6.0f;
    float cooling_plate_offset_below_tray = 2.0f;
    float enclosure_wall_thickness = 3.0f;
    float enclosure_floor_thickness = 4.0f;
    float enclosure_floor_offset = 4.0f;
    float enclosure_clearance_x = 6.0f;
    float enclosure_clearance_z = 6.0f;
    float module_tray_base_thickness = 3.0f;
    float module_tray_wall_thickness = 2.5f;
    float module_tray_wall_height = 6.0f;
    float cell_seating_offset = 1.0f;
    float support_rib_thickness = 2.0f;
    float support_rib_height = 4.0f;
    float module_tray_margin_x = 8.0f;
    float module_tray_margin_z = 10.0f;
};

struct ElectricalConfig
{
    double cell_nominal_voltage = 3.6;
    double cell_capacity_ah = 3.35;
    double discharge_current_a = 5.0;
    double internal_resistance_ohm = 0.035;
    double initial_soc = 1.0;
};

struct ThermalConfig
{
    double ambient_temp_c = 25.0;
    double pack_mass_kg = 1.0;
    double cooling_coeff_w_per_k = 1.0;
};

struct DerivedMetrics
{
    double pack_voltage = 14.4;
    double pack_capacity_ah = 6.7;
};

struct BatteryCadConfig
{
    PackLayoutConfig layout;
    ElectricalConfig electrical;
    ThermalConfig thermal;
    DerivedMetrics metrics;
};

} // namespace cad::battery
