#include "MeshLoader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>

namespace cad::io {

bool MeshLoader::loadBinaryStl(const std::string& path, TriangleMesh& mesh)
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

    mesh.vertices.clear();
    mesh.vertices.reserve(static_cast<std::size_t>(triangle_count) * 3);

    math::Vec3 min_v{1.0e9f, 1.0e9f, 1.0e9f};
    math::Vec3 max_v{-1.0e9f, -1.0e9f, -1.0e9f};

    for (std::uint32_t i = 0; i < triangle_count; ++i) {
        float normal[3];
        float points[9];
        std::uint16_t attribute = 0;
        input.read(reinterpret_cast<char*>(normal), sizeof(normal));
        input.read(reinterpret_cast<char*>(points), sizeof(points));
        input.read(reinterpret_cast<char*>(&attribute), sizeof(attribute));
        if (!input) {
            mesh.vertices.clear();
            return false;
        }

        for (int v = 0; v < 3; ++v) {
            math::Vec3 point{points[v * 3], points[v * 3 + 1], points[v * 3 + 2]};
            min_v.x = std::min(min_v.x, point.x);
            min_v.y = std::min(min_v.y, point.y);
            min_v.z = std::min(min_v.z, point.z);
            max_v.x = std::max(max_v.x, point.x);
            max_v.y = std::max(max_v.y, point.y);
            max_v.z = std::max(max_v.z, point.z);
            mesh.vertices.push_back(point);
        }
    }

    const math::Vec3 center{
        (min_v.x + max_v.x) * 0.5f,
        (min_v.y + max_v.y) * 0.5f,
        (min_v.z + max_v.z) * 0.5f
    };
    const math::Vec3 size{
        max_v.x - min_v.x,
        max_v.y - min_v.y,
        max_v.z - min_v.z
    };
    const float max_dim = std::max(size.x, std::max(size.y, size.z));
    if (max_dim <= 0.0f) {
        mesh.vertices.clear();
        return false;
    }

    const float target_height = 220.0f;
    const float scale = target_height / std::max(1.0f, size.y);
    for (math::Vec3& point : mesh.vertices) {
        point = math::mul(math::sub(point, center), scale);
    }

    return true;
}

} // namespace cad::io
