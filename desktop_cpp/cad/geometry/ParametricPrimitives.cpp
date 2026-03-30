#include "ParametricPrimitives.h"

#include <algorithm>
#include <cmath>

namespace cad::geometry {
namespace {

using cad::math::Vec3;

void appendTriangle(GeometryBuffer& geometry, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& color, SurfaceLayer layer)
{
    geometry.triangles.push_back({a, b, c, color, layer});
}

void appendQuad(GeometryBuffer& geometry, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& color, SurfaceLayer layer)
{
    appendTriangle(geometry, a, b, c, color, layer);
    appendTriangle(geometry, a, c, d, color, layer);
}

} // namespace

void appendBox(GeometryBuffer& geometry, const Vec3& center, const Vec3& size, const Vec3& color, SurfaceLayer layer)
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

    appendQuad(geometry, p000, p100, p110, p010, color, layer);
    appendQuad(geometry, p101, p001, p011, p111, color, layer);
    appendQuad(geometry, p001, p000, p010, p011, color, layer);
    appendQuad(geometry, p100, p101, p111, p110, color, layer);
    appendQuad(geometry, p010, p110, p111, p011, cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.07f), layer);
    appendQuad(geometry, p001, p101, p100, p000, cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.14f), layer);
}

void appendCylinder(GeometryBuffer& geometry, const Vec3& center, float radius, float height, int segments, const Vec3& color, SurfaceLayer layer)
{
    const float half_height = height * 0.5f;
    const Vec3 top_center{center.x, center.y + half_height, center.z};
    const Vec3 bottom_center{center.x, center.y - half_height, center.z};

    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * cad::math::kPi;
        const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * 2.0f * cad::math::kPi;
        const Vec3 p0{radius * std::cos(a0), 0.0f, radius * std::sin(a0)};
        const Vec3 p1{radius * std::cos(a1), 0.0f, radius * std::sin(a1)};

        const float light = 0.08f + 0.15f * std::max(0.0f, std::cos(a0 - 0.4f));
        const Vec3 side_color = cad::math::mix(color, {1.0f, 1.0f, 1.0f}, light);
        appendQuad(
            geometry,
            cad::math::add(top_center, p0),
            cad::math::add(top_center, p1),
            cad::math::add(bottom_center, p1),
            cad::math::add(bottom_center, p0),
            side_color,
            layer
        );
        appendTriangle(
            geometry,
            top_center,
            cad::math::add(top_center, p1),
            cad::math::add(top_center, p0),
            cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.18f),
            layer
        );
        appendTriangle(
            geometry,
            bottom_center,
            cad::math::add(bottom_center, p0),
            cad::math::add(bottom_center, p1),
            cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.14f),
            layer
        );
    }
}

void appendRing(GeometryBuffer& geometry, const Vec3& center, float outer_radius, float inner_radius, float height, int segments, const Vec3& color, SurfaceLayer layer)
{
    const float half_height = height * 0.5f;
    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * cad::math::kPi;
        const float a1 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * 2.0f * cad::math::kPi;

        const Vec3 outer_top0{center.x + outer_radius * std::cos(a0), center.y + half_height, center.z + outer_radius * std::sin(a0)};
        const Vec3 outer_top1{center.x + outer_radius * std::cos(a1), center.y + half_height, center.z + outer_radius * std::sin(a1)};
        const Vec3 outer_bottom0{outer_top0.x, center.y - half_height, outer_top0.z};
        const Vec3 outer_bottom1{outer_top1.x, center.y - half_height, outer_top1.z};
        const Vec3 inner_top0{center.x + inner_radius * std::cos(a0), center.y + half_height, center.z + inner_radius * std::sin(a0)};
        const Vec3 inner_top1{center.x + inner_radius * std::cos(a1), center.y + half_height, center.z + inner_radius * std::sin(a1)};
        const Vec3 inner_bottom0{inner_top0.x, center.y - half_height, inner_top0.z};
        const Vec3 inner_bottom1{inner_top1.x, center.y - half_height, inner_top1.z};

        appendQuad(geometry, outer_top0, outer_top1, inner_top1, inner_top0, color, layer);
        appendQuad(geometry, outer_bottom1, outer_bottom0, inner_bottom0, inner_bottom1, cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.08f), layer);
        appendQuad(geometry, outer_top0, outer_bottom0, outer_bottom1, outer_top1, cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.03f), layer);
        appendQuad(geometry, inner_top1, inner_bottom1, inner_bottom0, inner_top0, cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.08f), layer);
    }
}

void appendOpenTopTray(
    GeometryBuffer& geometry,
    const Vec3& seating_plane_center,
    const Vec3& footprint,
    float base_thickness,
    float wall_thickness,
    float wall_height,
    const Vec3& color,
    SurfaceLayer layer
)
{
    appendBox(
        geometry,
        {
            seating_plane_center.x,
            seating_plane_center.y - base_thickness * 0.5f,
            seating_plane_center.z
        },
        {footprint.x, base_thickness, footprint.z},
        cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.04f),
        layer
    );

    const float side_height = std::max(wall_height, 2.0f);
    const float half_width = footprint.x * 0.5f;
    const float half_depth = footprint.z * 0.5f;
    const float wall_center_y = seating_plane_center.y + side_height * 0.5f;

    appendBox(
        geometry,
        {seating_plane_center.x - half_width + wall_thickness * 0.5f, wall_center_y, seating_plane_center.z},
        {wall_thickness, side_height, footprint.z},
        color,
        layer
    );
    appendBox(
        geometry,
        {seating_plane_center.x + half_width - wall_thickness * 0.5f, wall_center_y, seating_plane_center.z},
        {wall_thickness, side_height, footprint.z},
        color,
        layer
    );
    appendBox(
        geometry,
        {seating_plane_center.x, wall_center_y, seating_plane_center.z - half_depth + wall_thickness * 0.5f},
        {footprint.x - wall_thickness * 2.0f, side_height, wall_thickness},
        color,
        layer
    );
    appendBox(
        geometry,
        {seating_plane_center.x, wall_center_y, seating_plane_center.z + half_depth - wall_thickness * 0.5f},
        {footprint.x - wall_thickness * 2.0f, side_height, wall_thickness},
        color,
        layer
    );
}

void appendOpenShell(
    GeometryBuffer& geometry,
    const Vec3& center,
    const Vec3& outer_size,
    float wall_thickness,
    float floor_thickness,
    float wall_height,
    const Vec3& color,
    SurfaceLayer layer
)
{
    appendBox(
        geometry,
        {center.x, center.y - outer_size.y * 0.5f + floor_thickness * 0.5f, center.z},
        {outer_size.x, floor_thickness, outer_size.z},
        cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.04f),
        layer
    );

    const float effective_wall_height = std::min(wall_height, outer_size.y - floor_thickness);
    const float wall_center_y = center.y - outer_size.y * 0.5f + floor_thickness + effective_wall_height * 0.5f;
    const float half_width = outer_size.x * 0.5f;
    const float half_depth = outer_size.z * 0.5f;

    appendBox(geometry, {center.x - half_width + wall_thickness * 0.5f, wall_center_y, center.z}, {wall_thickness, effective_wall_height, outer_size.z}, color, layer);
    appendBox(geometry, {center.x + half_width - wall_thickness * 0.5f, wall_center_y, center.z}, {wall_thickness, effective_wall_height, outer_size.z}, color, layer);
    appendBox(geometry, {center.x, wall_center_y, center.z - half_depth + wall_thickness * 0.5f}, {outer_size.x - wall_thickness * 2.0f, effective_wall_height, wall_thickness}, color, layer);
    appendBox(geometry, {center.x, wall_center_y, center.z + half_depth - wall_thickness * 0.5f}, {outer_size.x - wall_thickness * 2.0f, effective_wall_height, wall_thickness}, color, layer);
}

void appendCylindricalCell(
    GeometryBuffer& geometry,
    const Vec3& center,
    const CylindricalCellProfile& profile,
    int segments,
    const Vec3& body_color,
    const Vec3& cap_color,
    const Vec3& insulator_color,
    SurfaceLayer layer
)
{
    const float half_body_height = profile.body_height * 0.5f;
    if (segments <= 8) {
        appendCylinder(geometry, center, profile.body_radius, profile.body_height, segments, body_color, layer);
        const Vec3 top_cap_center{center.x, center.y + half_body_height + profile.top_cap_shoulder_height * 0.5f, center.z};
        appendCylinder(geometry, top_cap_center, profile.top_cap_outer_radius, profile.top_cap_shoulder_height, segments, cap_color, layer);
        return;
    }

    appendCylinder(geometry, center, profile.body_radius, profile.body_height, segments, body_color, layer);

    const Vec3 top_cap_center{center.x, center.y + half_body_height + profile.top_cap_shoulder_height * 0.5f, center.z};
    appendCylinder(geometry, top_cap_center, profile.top_cap_outer_radius, profile.top_cap_shoulder_height, segments, cap_color, layer);

    const float shoulder_top_y = center.y + half_body_height + profile.top_cap_shoulder_height;
    appendRing(
        geometry,
        {center.x, shoulder_top_y - profile.top_cap_shoulder_height * 0.15f, center.z},
        std::min(profile.top_cap_outer_radius, profile.body_radius * 0.98f),
        std::min(profile.top_cap_inner_radius, profile.top_cap_outer_radius - 0.5f),
        std::max(0.2f, profile.top_cap_shoulder_height * 0.35f),
        segments,
        cad::math::mix(cap_color, {0.0f, 0.0f, 0.0f}, 0.10f),
        layer
    );

    const float ring_y = shoulder_top_y + profile.insulator_height * 0.5f;
    appendRing(
        geometry,
        {center.x, ring_y, center.z},
        std::min(profile.insulator_outer_radius, profile.top_cap_outer_radius * 0.98f),
        std::min(profile.insulator_inner_radius, profile.insulator_outer_radius - 1.0f),
        profile.insulator_height,
        segments,
        insulator_color,
        layer
    );

    const Vec3 terminal_center{
        center.x,
        shoulder_top_y + profile.terminal_height * 0.5f,
        center.z
    };
    appendCylinder(
        geometry,
        terminal_center,
        std::min(profile.terminal_radius, profile.insulator_inner_radius * 0.95f),
        profile.terminal_height,
        segments,
        cad::math::mix(cap_color, {1.0f, 1.0f, 1.0f}, 0.05f),
        layer
    );
}

} // namespace cad::geometry
