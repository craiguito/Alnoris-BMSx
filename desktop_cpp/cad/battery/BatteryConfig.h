#pragma once

#include <string>

namespace cad::battery {

struct PackLayoutConfig
{
    std::string preset_name = "Custom";
    int cells_in_series = 4;
    int cells_in_parallel = 2;
    float cell_radius = 28.0f;
    float cell_height = 220.0f;
    float x_spacing = 92.0f;
    float z_spacing = 114.0f;
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
