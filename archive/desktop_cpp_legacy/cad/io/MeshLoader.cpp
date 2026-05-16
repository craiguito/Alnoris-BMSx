#include "MeshLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <limits>
#include <sstream>

namespace cad::io {
namespace {

void clearMesh(TriangleMesh& mesh)
{
    mesh.vertices.clear();
    mesh.source_center = {};
    mesh.source_size = {};
}

void finalizeMeshBounds(TriangleMesh& mesh, const math::Vec3& min_v, const math::Vec3& max_v)
{
    mesh.source_center = {
        (min_v.x + max_v.x) * 0.5f,
        (min_v.y + max_v.y) * 0.5f,
        (min_v.z + max_v.z) * 0.5f
    };
    mesh.source_size = {
        max_v.x - min_v.x,
        max_v.y - min_v.y,
        max_v.z - min_v.z
    };
}

bool sourceBoundsAreUsable(const TriangleMesh& mesh)
{
    return mesh.source_size.x > 0.0f || mesh.source_size.y > 0.0f || mesh.source_size.z > 0.0f;
}

bool readBinaryStl(const std::string& path, std::uint32_t triangle_count, TriangleMesh& mesh)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return false;
    }

    input.seekg(84, std::ios::beg);
    if (!input) {
        return false;
    }

    clearMesh(mesh);
    mesh.vertices.reserve(static_cast<std::size_t>(triangle_count) * 3);

    math::Vec3 min_v{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    math::Vec3 max_v{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    for (std::uint32_t i = 0; i < triangle_count; ++i) {
        float normal[3];
        float points[9];
        std::uint16_t attribute = 0;
        input.read(reinterpret_cast<char*>(normal), sizeof(normal));
        input.read(reinterpret_cast<char*>(points), sizeof(points));
        input.read(reinterpret_cast<char*>(&attribute), sizeof(attribute));
        if (!input) {
            clearMesh(mesh);
            return false;
        }

        for (int vertex_index = 0; vertex_index < 3; ++vertex_index) {
            const math::Vec3 point{
                points[vertex_index * 3],
                points[vertex_index * 3 + 1],
                points[vertex_index * 3 + 2]
            };
            min_v.x = std::min(min_v.x, point.x);
            min_v.y = std::min(min_v.y, point.y);
            min_v.z = std::min(min_v.z, point.z);
            max_v.x = std::max(max_v.x, point.x);
            max_v.y = std::max(max_v.y, point.y);
            max_v.z = std::max(max_v.z, point.z);
            mesh.vertices.push_back(point);
        }
    }

    finalizeMeshBounds(mesh, min_v, max_v);
    if (!sourceBoundsAreUsable(mesh)) {
        clearMesh(mesh);
        return false;
    }

    return true;
}

bool readAsciiStl(const std::string& path, TriangleMesh& mesh)
{
    std::ifstream input(path);
    if (!input) {
        return false;
    }

    clearMesh(mesh);
    math::Vec3 min_v{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    math::Vec3 max_v{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    std::string token;
    bool saw_solid = false;
    while (input >> token) {
        std::transform(token.begin(), token.end(), token.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        if (!saw_solid && token == "solid") {
            saw_solid = true;
            continue;
        }

        if (token != "vertex") {
            continue;
        }

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (!(input >> x >> y >> z)) {
            clearMesh(mesh);
            return false;
        }

        const math::Vec3 point{x, y, z};
        min_v.x = std::min(min_v.x, point.x);
        min_v.y = std::min(min_v.y, point.y);
        min_v.z = std::min(min_v.z, point.z);
        max_v.x = std::max(max_v.x, point.x);
        max_v.y = std::max(max_v.y, point.y);
        max_v.z = std::max(max_v.z, point.z);
        mesh.vertices.push_back(point);
    }

    if (!saw_solid || mesh.vertices.size() < 3 || (mesh.vertices.size() % 3) != 0) {
        clearMesh(mesh);
        return false;
    }

    finalizeMeshBounds(mesh, min_v, max_v);
    if (!sourceBoundsAreUsable(mesh)) {
        clearMesh(mesh);
        return false;
    }

    return true;
}

bool binaryTriangleLayoutMatchesFile(const std::string& path, std::uint32_t& triangle_count)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        return false;
    }

    const std::streamoff file_size = input.tellg();
    if (file_size < 84) {
        return false;
    }

    input.seekg(80, std::ios::beg);
    input.read(reinterpret_cast<char*>(&triangle_count), sizeof(triangle_count));
    if (!input || triangle_count == 0) {
        return false;
    }

    const std::uint64_t expected_size = 84ull + static_cast<std::uint64_t>(triangle_count) * 50ull;
    return expected_size == static_cast<std::uint64_t>(file_size);
}

} // namespace

bool MeshLoader::loadBinaryStl(const std::string& path, TriangleMesh& mesh)
{
    std::uint32_t triangle_count = 0;
    if (binaryTriangleLayoutMatchesFile(path, triangle_count) && readBinaryStl(path, triangle_count, mesh)) {
        return true;
    }

    return readAsciiStl(path, mesh);
}

} // namespace cad::io
