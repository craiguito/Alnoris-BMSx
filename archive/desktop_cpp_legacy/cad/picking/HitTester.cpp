#include "HitTester.h"

#include <algorithm>
#include <cmath>

namespace cad::picking {
namespace {

bool pickableHitsPoint(const render::ScreenPickable& pickable, float x, float y)
{
    const float dx = x - pickable.x;
    const float dy = y - pickable.y;
    if (pickable.shape == render::ScreenPickable::Shape::Rectangle) {
        return std::abs(dx) <= pickable.half_width && std::abs(dy) <= pickable.half_height;
    }

    const float radius = std::max(pickable.half_width, pickable.half_height);
    return (dx * dx) + (dy * dy) <= (radius * radius * 1.15f);
}

bool pickableRanksAhead(const render::ScreenPickable& candidate, const render::ScreenPickable& current_best)
{
    if (candidate.depth != current_best.depth) {
        return candidate.depth < current_best.depth;
    }
    if (candidate.selection_priority != current_best.selection_priority) {
        return candidate.selection_priority > current_best.selection_priority;
    }
    return candidate.entity_id.value < current_best.entity_id.value;
}

} // namespace

core::EntityId HitTester::hitTestEntity(const render::RenderPacket& packet, float x, float y) const
{
    const render::ScreenPickable* best_hit = nullptr;

    for (const auto& pickable : packet.pickables) {
        if (!pickableHitsPoint(pickable, x, y)) {
            continue;
        }

        if (best_hit == nullptr || pickableRanksAhead(pickable, *best_hit)) {
            best_hit = &pickable;
        }
    }
    return best_hit != nullptr ? best_hit->entity_id : core::EntityId{};
}

} // namespace cad::picking
