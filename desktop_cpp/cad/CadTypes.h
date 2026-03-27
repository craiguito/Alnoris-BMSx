#pragma once

#include <array>
#include <string>
#include <vector>

namespace cad {

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Vertex
{
    Vec3 position;
    Vec3 color;
};

struct ScreenCell
{
    int index = 0;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    float depth = 0.0f;
};

struct PackConfig
{
    std::string preset_name = "Custom";
    int cells_in_series = 4;
    int cells_in_parallel = 2;
    double cell_nominal_voltage = 3.6;
    double cell_capacity_ah = 3.35;
    double ambient_temp_c = 25.0;
    double internal_resistance_ohm = 0.035;
    double discharge_current_a = 5.0;
    double pack_mass_kg = 1.0;
    double cooling_coeff_w_per_k = 1.0;
    double initial_soc = 1.0;
    double pack_voltage = 14.4;
    double pack_capacity_ah = 6.7;
};

struct FrameData
{
    std::vector<Vertex> triangles;
    std::vector<Vertex> lines;
    std::vector<ScreenCell> screen_cells;
    std::array<float, 16> mvp{};
};

} // namespace cad
