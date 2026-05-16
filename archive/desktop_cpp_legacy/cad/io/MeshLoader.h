#pragma once

#include "../math/CadMath.h"

#include <string>
#include <vector>

namespace cad::io {

struct TriangleMesh
{
    std::vector<math::Vec3> vertices;
    math::Vec3 source_center{};
    math::Vec3 source_size{};

    [[nodiscard]] bool hasSourceBounds() const
    {
        return source_size.x > 0.0f || source_size.y > 0.0f || source_size.z > 0.0f;
    }
};

class MeshLoader
{
public:
    static bool loadBinaryStl(const std::string& path, TriangleMesh& mesh);
};

} // namespace cad::io
