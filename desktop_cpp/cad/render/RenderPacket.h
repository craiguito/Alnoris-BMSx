#pragma once

#include "../core/EntityId.h"
#include "../math/CadMath.h"

#include <array>
#include <vector>

namespace cad::render {

struct RenderVertex
{
    math::Vec3 position;
    math::Vec3 color;
};

struct ScreenPickable
{
    core::EntityId entity_id{};
    float x = 0.0f;
    float y = 0.0f;
    float radius = 0.0f;
    float depth = 0.0f;
};

struct RenderPacket
{
    std::vector<RenderVertex> triangles;
    std::vector<RenderVertex> lines;
    std::vector<ScreenPickable> pickables;
    std::array<float, 16> mvp{};
};

} // namespace cad::render
