#include "RenderComposer.h"

#include <algorithm>

namespace cad::render {
namespace {

using cad::math::Mat4;
using cad::math::Vec3;
using cad::math::Vec4;

Vec3 temperatureColor(double temp_c)
{
    const float normalized = cad::math::clamp(static_cast<float>((temp_c - 20.0) / 28.0), 0.0f, 1.0f);
    return cad::math::mix({0.28f, 0.73f, 1.0f}, {1.0f, 0.42f, 0.25f}, normalized);
}

void appendTriangle(std::vector<RenderVertex>& vertices, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& color)
{
    vertices.push_back({a, color});
    vertices.push_back({b, color});
    vertices.push_back({c, color});
}

void appendQuad(std::vector<RenderVertex>& vertices, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& color)
{
    appendTriangle(vertices, a, b, c, color);
    appendTriangle(vertices, a, c, d, color);
}

void appendBox(std::vector<RenderVertex>& vertices, const Vec3& center, const Vec3& size, const Vec3& color)
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

    appendQuad(vertices, p000, p100, p110, p010, color);
    appendQuad(vertices, p101, p001, p011, p111, color);
    appendQuad(vertices, p001, p000, p010, p011, color);
    appendQuad(vertices, p100, p101, p111, p110, color);
    appendQuad(vertices, p010, p110, p111, p011, cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.08f));
    appendQuad(vertices, p001, p101, p100, p000, cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.14f));
}

void appendCylinder(std::vector<RenderVertex>& vertices, const Vec3& center, float radius, float height, int segments, const Vec3& color)
{
    const float half_height = height * 0.5f;
    const Vec3 top_center{center.x, center.y + half_height, center.z};
    const Vec3 bottom_center{center.x, center.y - half_height, center.z};

    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / segments) * 2.0f * cad::math::kPi;
        const float a1 = (static_cast<float>(i + 1) / segments) * 2.0f * cad::math::kPi;
        const Vec3 p0{radius * std::cos(a0), 0.0f, radius * std::sin(a0)};
        const Vec3 p1{radius * std::cos(a1), 0.0f, radius * std::sin(a1)};

        const Vec3 side = cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.10f + 0.18f * std::max(0.0f, std::cos(a0)));
        appendQuad(vertices, cad::math::add(top_center, p0), cad::math::add(top_center, p1), cad::math::add(bottom_center, p1), cad::math::add(bottom_center, p0), side);
        appendTriangle(vertices, top_center, cad::math::add(top_center, p1), cad::math::add(top_center, p0), cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.24f));
        appendTriangle(vertices, bottom_center, cad::math::add(bottom_center, p0), cad::math::add(bottom_center, p1), cad::math::mix(color, {0.0f, 0.0f, 0.0f}, 0.18f));
    }
}

void appendMesh(std::vector<RenderVertex>& vertices, const io::TriangleMesh& mesh, const Vec3& offset, const Vec3& color)
{
    for (const Vec3& point : mesh.vertices) {
        vertices.push_back({cad::math::add(point, offset), color});
    }
}

struct ProjectionResult
{
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    bool valid = false;
};

ProjectionResult projectPoint(const Mat4& mvp, const Vec3& p, int width, int height)
{
    const Vec4 clip = cad::math::multiply(mvp, Vec4{p.x, p.y, p.z, 1.0f});
    if (clip.w <= 0.0001f) {
        return {};
    }

    const float ndc_x = clip.x / clip.w;
    const float ndc_y = clip.y / clip.w;
    const float ndc_z = clip.z / clip.w;
    return {
        (ndc_x * 0.5f + 0.5f) * width,
        (1.0f - (ndc_y * 0.5f + 0.5f)) * height,
        ndc_z,
        true
    };
}

void appendBoxPickable(
    std::vector<ScreenPickable>& pickables,
    core::EntityId entity_id,
    const Mat4& mvp,
    const Vec3& center,
    const Vec3& size,
    int width,
    int height
)
{
    const Vec3 half{size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};
    const Vec3 corners[] = {
        {center.x - half.x, center.y - half.y, center.z - half.z},
        {center.x - half.x, center.y - half.y, center.z + half.z},
        {center.x - half.x, center.y + half.y, center.z - half.z},
        {center.x - half.x, center.y + half.y, center.z + half.z},
        {center.x + half.x, center.y - half.y, center.z - half.z},
        {center.x + half.x, center.y - half.y, center.z + half.z},
        {center.x + half.x, center.y + half.y, center.z - half.z},
        {center.x + half.x, center.y + half.y, center.z + half.z}
    };

    float min_x = static_cast<float>(width);
    float min_y = static_cast<float>(height);
    float max_x = 0.0f;
    float max_y = 0.0f;
    float depth_accumulator = 0.0f;
    int valid_count = 0;

    for (const Vec3& corner : corners) {
        const ProjectionResult projected = projectPoint(mvp, corner, width, height);
        if (!projected.valid) {
            continue;
        }
        min_x = std::min(min_x, projected.x);
        min_y = std::min(min_y, projected.y);
        max_x = std::max(max_x, projected.x);
        max_y = std::max(max_y, projected.y);
        depth_accumulator += projected.depth;
        ++valid_count;
    }

    if (valid_count < 2) {
        return;
    }

    pickables.push_back({
        entity_id,
        ScreenPickable::Shape::Rectangle,
        (min_x + max_x) * 0.5f,
        (min_y + max_y) * 0.5f,
        std::max(5.0f, (max_x - min_x) * 0.5f),
        std::max(5.0f, (max_y - min_y) * 0.5f),
        depth_accumulator / static_cast<float>(valid_count)
    });
}

void appendWireBox(std::vector<RenderVertex>& lines, const Vec3& center, const Vec3& size, const Vec3& color)
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

    const Vec3 pairs[] = {
        p000, p001, p000, p010, p000, p100,
        p111, p110, p111, p101, p111, p011,
        p001, p011, p001, p101,
        p010, p011, p010, p110,
        p100, p101, p100, p110
    };
    for (std::size_t i = 0; i < std::size(pairs); i += 2) {
        lines.push_back({pairs[i], color});
        lines.push_back({pairs[i + 1], color});
    }
}

} // namespace

RenderPacket RenderComposer::compose(
    const core::CadDocument& document,
    const battery::BatteryVisualizationOverlay& overlay,
    const camera::Camera& camera,
    const io::TriangleMesh* cell_mesh,
    int viewport_width,
    int viewport_height
) const
{
    RenderPacket packet;

    const Mat4 projection = camera.projectionMatrix(static_cast<float>(std::max(1, viewport_width)) / static_cast<float>(std::max(1, viewport_height)));
    const Mat4 view = camera.viewMatrix();
    const Mat4 mvp = cad::math::multiply(projection, view);
    packet.mvp = mvp.m;

    const Vec3 grid_color{0.16f, 0.23f, 0.30f};
    const int grid_extent = 14;
    const float grid_step = 70.0f;
    for (int i = -grid_extent; i <= grid_extent; ++i) {
        const float offset = i * grid_step;
        packet.lines.push_back({{offset, -120.0f, -grid_extent * grid_step}, grid_color});
        packet.lines.push_back({{offset, -120.0f, grid_extent * grid_step}, grid_color});
        packet.lines.push_back({{-grid_extent * grid_step, -120.0f, offset}, grid_color});
        packet.lines.push_back({{grid_extent * grid_step, -120.0f, offset}, grid_color});
    }

    for (const battery::CoolingPlateEntity& plate : document.coolingPlates()) {
        appendBox(packet.triangles, plate.center, plate.size, {0.08f, 0.16f, 0.24f});
        appendBoxPickable(packet.pickables, plate.id, mvp, plate.center, plate.size, viewport_width, viewport_height);
    }
    for (const battery::BusbarEntity& busbar : document.busbars()) {
        appendBox(packet.triangles, busbar.center, busbar.size, {0.86f, 0.68f, 0.30f});
        appendBoxPickable(packet.pickables, busbar.id, mvp, busbar.center, busbar.size, viewport_width, viewport_height);
    }
    for (const battery::ModuleBoundaryEntity& boundary : document.moduleBoundaries()) {
        appendWireBox(packet.lines, boundary.center, boundary.size, {0.25f, 0.45f, 0.86f});
        appendBoxPickable(packet.pickables, boundary.id, mvp, boundary.center, boundary.size, viewport_width, viewport_height);
    }
    for (const battery::PackEnclosureEntity& enclosure : document.packEnclosures()) {
        appendWireBox(packet.lines, enclosure.center, enclosure.size, {0.42f, 0.48f, 0.54f});
        appendBoxPickable(packet.pickables, enclosure.id, mvp, enclosure.center, enclosure.size, viewport_width, viewport_height);
    }

    for (const battery::CellEntity& cell : document.cells()) {
        Vec3 color = {0.68f, 0.71f, 0.76f};
        const auto temp_it = overlay.cell_temperature_c.find(cell.id);
        if (temp_it != overlay.cell_temperature_c.end()) {
            color = temperatureColor(temp_it->second);
        }
        if (document.selection().primary == cell.id) {
            color = cad::math::mix(color, {1.0f, 1.0f, 1.0f}, 0.32f);
        }

        if (cell_mesh != nullptr && !cell_mesh->vertices.empty()) {
            appendMesh(packet.triangles, *cell_mesh, cell.position, color);
        } else {
            appendCylinder(packet.triangles, cell.position, cell.radius, cell.height, 28, color);
            appendCylinder(
                packet.triangles,
                {cell.position.x, cell.position.y + (cell.height * 0.52f), cell.position.z},
                cell.radius * 0.34f,
                10.0f,
                24,
                document.selection().primary == cell.id ? Vec3{0.96f, 0.99f, 1.0f} : Vec3{0.84f, 0.90f, 0.96f}
            );
        }

        const ProjectionResult center = projectPoint(mvp, cell.position, viewport_width, viewport_height);
        const ProjectionResult edge = projectPoint(
            mvp,
            {cell.position.x + (cell.radius + 4.0f), cell.position.y, cell.position.z},
            viewport_width,
            viewport_height
        );
        if (center.valid && edge.valid) {
            packet.pickables.push_back({
                cell.id,
                ScreenPickable::Shape::Circle,
                center.x,
                center.y,
                std::max(6.0f, std::abs(edge.x - center.x)),
                std::max(6.0f, std::abs(edge.x - center.x)),
                center.depth
            });
        }
    }

    std::sort(packet.pickables.begin(), packet.pickables.end(), [](const ScreenPickable& a, const ScreenPickable& b) {
        return a.depth < b.depth;
    });

    packet.lines.push_back({{-900.0f, 0.0f, 0.0f}, {0.78f, 0.22f, 0.22f}});
    packet.lines.push_back({{900.0f, 0.0f, 0.0f}, {0.78f, 0.22f, 0.22f}});
    packet.lines.push_back({{0.0f, -200.0f, 0.0f}, {0.24f, 0.72f, 0.36f}});
    packet.lines.push_back({{0.0f, 340.0f, 0.0f}, {0.24f, 0.72f, 0.36f}});
    packet.lines.push_back({{0.0f, 0.0f, -900.0f}, {0.25f, 0.45f, 0.86f}});
    packet.lines.push_back({{0.0f, 0.0f, 900.0f}, {0.25f, 0.45f, 0.86f}});

    return packet;
}

} // namespace cad::render
