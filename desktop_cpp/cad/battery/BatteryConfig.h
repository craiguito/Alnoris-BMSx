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
    float cell_radius = 9.0f;
    float cell_height = 65.0f;
    float cell_width = 18.0f;
    float cell_depth = 18.0f;
    float x_spacing = 23.0f;
    float z_spacing = 23.0f;
    float module_gap_x = 28.0f;
    float busbar_thickness = 2.2f;
    float busbar_tab_width = 8.0f;
    float busbar_tab_depth = 10.0f;
    float cooling_channel_thickness = 5.0f;
    float cooling_plate_margin_x = 16.0f;
    float cooling_plate_margin_z = 18.0f;
    float enclosure_wall_thickness = 2.5f;
    float enclosure_floor_thickness = 3.0f;
    float module_tray_wall_height = 10.0f;
    float module_tray_margin_x = 8.0f;
    float module_tray_margin_z = 8.0f;
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
