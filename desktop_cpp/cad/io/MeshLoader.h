#pragma once

#include "../math/CadMath.h"

#include <string>
#include <vector>

namespace cad::io {

struct TriangleMesh
{
    std::vector<math::Vec3> vertices;
};

class MeshLoader
{
public:
    static bool loadBinaryStl(const std::string& path, TriangleMesh& mesh);
};

} // namespace cad::io
