#pragma once

#include "../math/CadMath.h"

#include <vector>

namespace cad::geometry {

struct ColoredTriangle
{
    math::Vec3 a{};
    math::Vec3 b{};
    math::Vec3 c{};
    math::Vec3 color{};
};

struct ColoredLine
{
    math::Vec3 a{};
    math::Vec3 b{};
    math::Vec3 color{};
};

struct GeometryBuffer
{
    std::vector<ColoredTriangle> triangles;
    std::vector<ColoredLine> lines;
};

struct CylindricalCellProfile
{
    float body_radius = 28.0f;
    float body_height = 220.0f;
    float cap_height = 7.0f;
    float cap_radius = 25.5f;
    float terminal_radius = 8.5f;
    float terminal_height = 3.6f;
    float insulator_outer_radius = 17.5f;
    float insulator_inner_radius = 10.0f;
    float insulator_height = 1.6f;
    float bottom_cap_height = 3.0f;
};

void appendBox(GeometryBuffer& geometry, const math::Vec3& center, const math::Vec3& size, const math::Vec3& color);
void appendCylinder(GeometryBuffer& geometry, const math::Vec3& center, float radius, float height, int segments, const math::Vec3& color);
void appendRing(GeometryBuffer& geometry, const math::Vec3& center, float outer_radius, float inner_radius, float height, int segments, const math::Vec3& color);
void appendOpenTopTray(
    GeometryBuffer& geometry,
    const math::Vec3& center,
    const math::Vec3& footprint,
    float base_thickness,
    float wall_thickness,
    float wall_height,
    const math::Vec3& color
);
void appendOpenShell(
    GeometryBuffer& geometry,
    const math::Vec3& center,
    const math::Vec3& outer_size,
    float wall_thickness,
    float floor_thickness,
    float wall_height,
    const math::Vec3& color
);
void appendCylindricalCell(
    GeometryBuffer& geometry,
    const math::Vec3& center,
    const CylindricalCellProfile& profile,
    int segments,
    const math::Vec3& body_color,
    const math::Vec3& cap_color,
    const math::Vec3& insulator_color
);

} // namespace cad::geometry
