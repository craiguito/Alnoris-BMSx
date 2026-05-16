#pragma once

#include "../math/CadMath.h"

#include <vector>

namespace cad::geometry {

enum class SurfaceLayer : unsigned char
{
    Background = 0,
    Support = 1,
    Cell = 2,
    Busbar = 3
};

struct ColoredTriangle
{
    math::Vec3 a{};
    math::Vec3 b{};
    math::Vec3 c{};
    math::Vec3 color{};
    SurfaceLayer layer = SurfaceLayer::Support;
};

struct ColoredLine
{
    math::Vec3 a{};
    math::Vec3 b{};
    math::Vec3 color{};
    SurfaceLayer layer = SurfaceLayer::Support;
};

struct GeometryBuffer
{
    std::vector<ColoredTriangle> triangles;
    std::vector<ColoredLine> lines;
};

struct CylindricalCellProfile
{
    float body_radius = 10.5f;
    float body_height = 70.0f;
    float top_cap_shoulder_height = 1.0f;
    float top_cap_outer_radius = 9.25f;
    float top_cap_inner_radius = 7.25f;
    float terminal_radius = 4.0f;
    float terminal_height = 1.4f;
    float insulator_outer_radius = 8.25f;
    float insulator_inner_radius = 4.5f;
    float insulator_height = 0.5f;
    float bottom_cap_height = 0.8f;
};

void appendBox(GeometryBuffer& geometry, const math::Vec3& center, const math::Vec3& size, const math::Vec3& color, SurfaceLayer layer = SurfaceLayer::Support);
void appendCylinder(GeometryBuffer& geometry, const math::Vec3& center, float radius, float height, int segments, const math::Vec3& color, SurfaceLayer layer = SurfaceLayer::Support);
void appendRing(GeometryBuffer& geometry, const math::Vec3& center, float outer_radius, float inner_radius, float height, int segments, const math::Vec3& color, SurfaceLayer layer = SurfaceLayer::Support);
void appendOpenTopTray(
    GeometryBuffer& geometry,
    const math::Vec3& seating_plane_center,
    const math::Vec3& footprint,
    float base_thickness,
    float wall_thickness,
    float wall_height,
    const math::Vec3& color,
    SurfaceLayer layer = SurfaceLayer::Support
);
void appendOpenShell(
    GeometryBuffer& geometry,
    const math::Vec3& center,
    const math::Vec3& outer_size,
    float wall_thickness,
    float floor_thickness,
    float wall_height,
    const math::Vec3& color,
    SurfaceLayer layer = SurfaceLayer::Background
);
void appendCylindricalCell(
    GeometryBuffer& geometry,
    const math::Vec3& center,
    const CylindricalCellProfile& profile,
    int segments,
    const math::Vec3& body_color,
    const math::Vec3& cap_color,
    const math::Vec3& insulator_color,
    SurfaceLayer layer = SurfaceLayer::Cell
);

} // namespace cad::geometry
