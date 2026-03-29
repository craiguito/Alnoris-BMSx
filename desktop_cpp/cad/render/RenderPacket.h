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
    unsigned char layer = 0;
};

struct ScreenPickable
{
    enum class Shape
    {
        Circle,
        Rectangle
    };

    core::EntityId entity_id{};
    Shape shape = Shape::Circle;
    int selection_priority = 0;
    float x = 0.0f;
    float y = 0.0f;
    float half_width = 0.0f;
    float half_height = 0.0f;
    float depth = 0.0f;
};

struct RenderPacket
{
    std::vector<RenderVertex> triangles;
    std::vector<RenderVertex> lines;
    std::vector<RenderVertex> selection_overlay_lines;
    std::vector<ScreenPickable> pickables;
    std::array<float, 16> mvp{};
};

} // namespace cad::render
