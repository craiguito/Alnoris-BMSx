#include "HitTester.h"

#include <algorithm>
#include <cmath>

namespace cad::picking {

core::EntityId HitTester::hitTestEntity(const render::RenderPacket& packet, float x, float y) const
{
    for (auto it = packet.pickables.rbegin(); it != packet.pickables.rend(); ++it) {
        const float dx = x - it->x;
        const float dy = y - it->y;
        bool hit = false;
        if (it->shape == render::ScreenPickable::Shape::Rectangle) {
            hit = std::abs(dx) <= it->half_width && std::abs(dy) <= it->half_height;
        } else {
            const float radius = std::max(it->half_width, it->half_height);
            hit = (dx * dx) + (dy * dy) <= (radius * radius * 1.15f);
        }
        if (hit) {
            return it->entity_id;
        }
    }
    return {};
}

} // namespace cad::picking
