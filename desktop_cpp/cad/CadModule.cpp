#include "CadModule.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>

namespace cad {
namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Mat4
{
    std::array<float, 16> m{};
};

float clamp_float(float value, float low, float high)
{
    return std::max(low, std::min(high, value));
}

Vec3 add(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 sub(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 mul(const Vec3& a, float scalar)
{
    return {a.x * scalar, a.y * scalar, a.z * scalar};
}

Vec3 mix(const Vec3& a, const Vec3& b, float amount)
{
    const float t = clamp_float(amount, 0.0f, 1.0f);
    return add(a, mul(sub(b, a), t));
}

Mat4 identity()
{
    Mat4 out{};
    out.m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return out;
}

Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 out{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k) {
                value += a.m[(row * 4) + k] * b.m[(k * 4) + col];
            }
            out.m[(row * 4) + col] = value;
        }
    }
    return out;
}

Vec4 multiply(const Mat4& m, const Vec4& v)
{
    return {
        (m.m[0] * v.x) + (m.m[1] * v.y) + (m.m[2] * v.z) + (m.m[3] * v.w),
        (m.m[4] * v.x) + (m.m[5] * v.y) + (m.m[6] * v.z) + (m.m[7] * v.w),
        (m.m[8] * v.x) + (m.m[9] * v.y) + (m.m[10] * v.z) + (m.m[11] * v.w),
        (m.m[12] * v.x) + (m.m[13] * v.y) + (m.m[14] * v.z) + (m.m[15] * v.w)
    };
}

Mat4 perspective(float fov_deg, float aspect, float near_plane, float far_plane)
{
    const float f = 1.0f / std::tan((fov_deg * kPi / 180.0f) * 0.5f);
    Mat4 out{};
    out.m = {
        f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, (far_plane + near_plane) / (near_plane - far_plane), (2.0f * far_plane * near_plane) / (near_plane - far_plane),
        0.0f, 0.0f, -1.0f, 0.0f
    };
    return out;
}

Mat4 translation(float x, float y, float z)
{
    Mat4 out = identity();
    out.m[3] = x;
    out.m[7] = y;
    out.m[11] = z;
    return out;
}

Mat4 rotation_x(float deg)
{
    const float r = deg * kPi / 180.0f;
    const float c = std::cos(r);
    const float s = std::sin(r);
    Mat4 out = identity();
    out.m[5] = c;
    out.m[6] = -s;
    out.m[9] = s;
    out.m[10] = c;
    return out;
}

Mat4 rotation_y(float deg)
{
    const float r = deg * kPi / 180.0f;
    const float c = std::cos(r);
    const float s = std::sin(r);
    Mat4 out = identity();
    out.m[0] = c;
    out.m[2] = s;
    out.m[8] = -s;
    out.m[10] = c;
    return out;
}

void append_triangle(std::vector<Vertex>& vertices, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& color)
{
    vertices.push_back({a, color});
    vertices.push_back({b, color});
    vertices.push_back({c, color});
}

void append_quad(std::vector<Vertex>& vertices, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& color)
{
    append_triangle(vertices, a, b, c, color);
    append_triangle(vertices, a, c, d, color);
}

void append_box(std::vector<Vertex>& vertices, const Vec3& center, const Vec3& size, const Vec3& color)
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

    append_quad(vertices, p000, p100, p110, p010, color);
    append_quad(vertices, p101, p001, p011, p111, color);
    append_quad(vertices, p001, p000, p010, p011, color);
    append_quad(vertices, p100, p101, p111, p110, color);
    append_quad(vertices, p010, p110, p111, p011, mix(color, {1.0f, 1.0f, 1.0f}, 0.08f));
    append_quad(vertices, p001, p101, p100, p000, mix(color, {0.0f, 0.0f, 0.0f}, 0.14f));
}

void append_cylinder(std::vector<Vertex>& vertices, const Vec3& center, float radius, float height, int segments, const Vec3& color)
{
    const float half_height = height * 0.5f;
    const Vec3 top_center{center.x, center.y + half_height, center.z};
    const Vec3 bottom_center{center.x, center.y - half_height, center.z};

    for (int i = 0; i < segments; ++i) {
        const float a0 = (static_cast<float>(i) / segments) * 2.0f * kPi;
        const float a1 = (static_cast<float>(i + 1) / segments) * 2.0f * kPi;
        const Vec3 p0{radius * std::cos(a0), 0.0f, radius * std::sin(a0)};
        const Vec3 p1{radius * std::cos(a1), 0.0f, radius * std::sin(a1)};

        const Vec3 side = mix(color, {1.0f, 1.0f, 1.0f}, 0.10f + 0.18f * std::max(0.0f, std::cos(a0)));
        append_quad(
            vertices,
            add(top_center, p0),
            add(top_center, p1),
            add(bottom_center, p1),
            add(bottom_center, p0),
            side
        );
        append_triangle(vertices, top_center, add(top_center, p1), add(top_center, p0), mix(color, {1.0f, 1.0f, 1.0f}, 0.24f));
        append_triangle(vertices, bottom_center, add(bottom_center, p0), add(bottom_center, p1), mix(color, {0.0f, 0.0f, 0.0f}, 0.18f));
    }
}

bool load_binary_stl_vertices(const std::string& path, std::vector<Vec3>& vertices)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    char header[80];
    input.read(header, sizeof(header));
    if (!input) {
        return false;
    }

    std::uint32_t triangle_count = 0;
    input.read(reinterpret_cast<char*>(&triangle_count), sizeof(triangle_count));
    if (!input || triangle_count == 0) {
        return false;
    }

    vertices.clear();
    vertices.reserve(static_cast<std::size_t>(triangle_count) * 3);

    Vec3 min_v{1.0e9f, 1.0e9f, 1.0e9f};
    Vec3 max_v{-1.0e9f, -1.0e9f, -1.0e9f};

    for (std::uint32_t i = 0; i < triangle_count; ++i) {
        float normal[3];
        float points[9];
        std::uint16_t attribute = 0;
        input.read(reinterpret_cast<char*>(normal), sizeof(normal));
        input.read(reinterpret_cast<char*>(points), sizeof(points));
        input.read(reinterpret_cast<char*>(&attribute), sizeof(attribute));
        if (!input) {
            vertices.clear();
            return false;
        }

        for (int v = 0; v < 3; ++v) {
            Vec3 point{points[v * 3], points[v * 3 + 1], points[v * 3 + 2]};
            min_v.x = std::min(min_v.x, point.x);
            min_v.y = std::min(min_v.y, point.y);
            min_v.z = std::min(min_v.z, point.z);
            max_v.x = std::max(max_v.x, point.x);
            max_v.y = std::max(max_v.y, point.y);
            max_v.z = std::max(max_v.z, point.z);
            vertices.push_back(point);
        }
    }

    const Vec3 center{
        (min_v.x + max_v.x) * 0.5f,
        (min_v.y + max_v.y) * 0.5f,
        (min_v.z + max_v.z) * 0.5f
    };
    const Vec3 size{
        max_v.x - min_v.x,
        max_v.y - min_v.y,
        max_v.z - min_v.z
    };
    const float max_dim = std::max(size.x, std::max(size.y, size.z));
    if (max_dim <= 0.0f) {
        vertices.clear();
        return false;
    }

    const float target_height = 220.0f;
    const float scale = target_height / std::max(1.0f, size.y);
    for (Vec3& point : vertices) {
        point = mul(sub(point, center), scale);
    }

    return true;
}

void append_mesh(std::vector<Vertex>& vertices, const std::vector<Vec3>& mesh, const Vec3& offset, const Vec3& color)
{
    for (const Vec3& point : mesh) {
        vertices.push_back({add(point, offset), color});
    }
}

} // namespace

Module::Module()
{
    rebuild_frame();
}

bool Module::set_cell_mesh_path(const std::string& path)
{
    std::vector<Vec3> vertices;
    if (!load_binary_stl_vertices(path, vertices)) {
        return false;
    }

    m_cell_mesh_vertices = std::move(vertices);
    rebuild_frame();
    return true;
}

void Module::set_viewport_size(int width, int height)
{
    m_viewport_width = std::max(1, width);
    m_viewport_height = std::max(1, height);
    rebuild_frame();
}

void Module::set_pack_config(const PackConfig& config)
{
    m_config = config;
    const int max_index = std::max(0, (m_config.cells_in_series * m_config.cells_in_parallel) - 1);
    m_selected_cell = std::min(m_selected_cell, max_index);
    rebuild_frame();
}

void Module::orbit(float delta_yaw_deg, float delta_pitch_deg)
{
    m_yaw_deg += delta_yaw_deg;
    m_pitch_deg = clamp_float(m_pitch_deg + delta_pitch_deg, -80.0f, 15.0f);
    rebuild_frame();
}

void Module::zoom(float delta)
{
    m_zoom = clamp_float(m_zoom + delta, 0.45f, 2.3f);
    rebuild_frame();
}

void Module::select_cell(int index)
{
    const int max_index = std::max(0, (m_config.cells_in_series * m_config.cells_in_parallel) - 1);
    m_selected_cell = std::max(0, std::min(index, max_index));
    rebuild_frame();
}

int Module::hit_test_cell(float x, float y) const
{
    for (auto it = m_frame.screen_cells.rbegin(); it != m_frame.screen_cells.rend(); ++it) {
        const float dx = x - it->x;
        const float dy = y - it->y;
        if ((dx * dx) + (dy * dy) <= (it->radius * it->radius * 1.15f)) {
            return it->index;
        }
    }
    return -1;
}

const FrameData& Module::frame_data() const
{
    return m_frame;
}

int Module::selected_cell() const
{
    return m_selected_cell;
}

double Module::estimated_cell_temp(int index) const
{
    const int series_count = std::max(1, m_config.cells_in_series);
    const int parallel_count = std::max(1, m_config.cells_in_parallel);
    const int series_index = index % series_count;
    const int branch_index = index / series_count;
    const double branch_current = m_config.discharge_current_a / static_cast<double>(parallel_count);
    const double ohmic_heat = branch_current * branch_current * m_config.internal_resistance_ohm;
    const double cooling_effect = std::max(0.15, m_config.cooling_coeff_w_per_k / 6.0);
    const double gradient = series_index * 0.9 + branch_index * 1.3;
    return m_config.ambient_temp_c + (ohmic_heat * 13.0 / cooling_effect) + gradient;
}

Vec3 Module::temperature_color(double temp_c) const
{
    const float normalized = clamp_float(static_cast<float>((temp_c - 20.0) / 28.0), 0.0f, 1.0f);
    return mix({0.28f, 0.73f, 1.0f}, {1.0f, 0.42f, 0.25f}, normalized);
}

void Module::rebuild_frame()
{
    m_frame.triangles.clear();
    m_frame.lines.clear();
    m_frame.screen_cells.clear();

    const Mat4 projection = perspective(42.0f, static_cast<float>(m_viewport_width) / static_cast<float>(m_viewport_height), 1.0f, 5000.0f);
    const Mat4 view = multiply(
        multiply(translation(0.0f, -80.0f, -1000.0f / m_zoom), rotation_x(m_pitch_deg)),
        rotation_y(m_yaw_deg)
    );
    const Mat4 world = identity();
    const Mat4 mvp = multiply(multiply(projection, view), world);
    m_frame.mvp = mvp.m;

    const Vec3 grid_color{0.16f, 0.23f, 0.30f};
    const int grid_extent = 14;
    const float grid_step = 70.0f;
    for (int i = -grid_extent; i <= grid_extent; ++i) {
        const float offset = i * grid_step;
        m_frame.lines.push_back({{offset, -120.0f, -grid_extent * grid_step}, grid_color});
        m_frame.lines.push_back({{offset, -120.0f, grid_extent * grid_step}, grid_color});
        m_frame.lines.push_back({{-grid_extent * grid_step, -120.0f, offset}, grid_color});
        m_frame.lines.push_back({{grid_extent * grid_step, -120.0f, offset}, grid_color});
    }

    append_box(m_frame.triangles, {0.0f, -128.0f, 0.0f}, {680.0f, 24.0f, 320.0f}, {0.08f, 0.16f, 0.24f});
    append_box(m_frame.triangles, {0.0f, 112.0f, -150.0f}, {700.0f, 12.0f, 16.0f}, {0.86f, 0.68f, 0.30f});
    append_box(m_frame.triangles, {0.0f, 112.0f, 150.0f}, {700.0f, 12.0f, 16.0f}, {0.86f, 0.68f, 0.30f});

    const int series_count = std::max(1, m_config.cells_in_series);
    const int parallel_count = std::max(1, m_config.cells_in_parallel);
    const float x_spacing = 92.0f;
    const float z_spacing = 114.0f;
    const float cell_radius = 28.0f;
    const float cell_height = 220.0f;

    for (int row = 0; row < parallel_count; ++row) {
        for (int col = 0; col < series_count; ++col) {
            const int index = row * series_count + col;
            const Vec3 center{
                (col - (series_count - 1) / 2.0f) * x_spacing,
                0.0f,
                (row - (parallel_count - 1) / 2.0f) * z_spacing
            };
            Vec3 color = temperature_color(estimated_cell_temp(index));
            if (index == m_selected_cell) {
                color = mix(color, {1.0f, 1.0f, 1.0f}, 0.32f);
            }
            if (!m_cell_mesh_vertices.empty()) {
                append_mesh(m_frame.triangles, m_cell_mesh_vertices, center, color);
            } else {
                append_cylinder(m_frame.triangles, center, cell_radius, cell_height, 28, color);
                append_cylinder(
                    m_frame.triangles,
                    {center.x, center.y + (cell_height * 0.52f), center.z},
                    cell_radius * 0.34f,
                    10.0f,
                    24,
                    index == m_selected_cell ? Vec3{0.96f, 0.99f, 1.0f} : Vec3{0.84f, 0.90f, 0.96f}
                );
            }

            const Vec4 clip_input{center.x, center.y, center.z, 1.0f};
            const Vec4 clip = multiply(mvp, clip_input);
            if (clip.w <= 0.0f) {
                continue;
            }
            const float ndc_x = clip.x / clip.w;
            const float ndc_y = clip.y / clip.w;
            const float screen_x = (ndc_x * 0.5f + 0.5f) * m_viewport_width;
            const float screen_y = (1.0f - (ndc_y * 0.5f + 0.5f)) * m_viewport_height;

            const Vec4 edge_input{center.x + 32.0f, center.y, center.z, 1.0f};
            const Vec4 edge_clip = multiply(mvp, edge_input);
            const float edge_ndc_x = edge_clip.x / edge_clip.w;
            const float edge_screen_x = (edge_ndc_x * 0.5f + 0.5f) * m_viewport_width;
            const float radius = std::max(6.0f, std::abs(edge_screen_x - screen_x));

            m_frame.screen_cells.push_back({index, screen_x, screen_y, radius, clip.z / clip.w});
        }
    }

    std::sort(m_frame.screen_cells.begin(), m_frame.screen_cells.end(), [](const ScreenCell& a, const ScreenCell& b) {
        return a.depth < b.depth;
    });

    m_frame.lines.push_back({{-900.0f, 0.0f, 0.0f}, {0.78f, 0.22f, 0.22f}});
    m_frame.lines.push_back({{900.0f, 0.0f, 0.0f}, {0.78f, 0.22f, 0.22f}});
    m_frame.lines.push_back({{0.0f, -200.0f, 0.0f}, {0.24f, 0.72f, 0.36f}});
    m_frame.lines.push_back({{0.0f, 340.0f, 0.0f}, {0.24f, 0.72f, 0.36f}});
    m_frame.lines.push_back({{0.0f, 0.0f, -900.0f}, {0.25f, 0.45f, 0.86f}});
    m_frame.lines.push_back({{0.0f, 0.0f, 900.0f}, {0.25f, 0.45f, 0.86f}});
}

} // namespace cad
