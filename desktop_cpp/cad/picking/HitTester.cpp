#include "HitTester.h"

namespace cad::picking {

core::EntityId HitTester::hitTestEntity(const render::RenderPacket& packet, float x, float y) const
{
    for (auto it = packet.pickables.rbegin(); it != packet.pickables.rend(); ++it) {
        const float dx = x - it->x;
        const float dy = y - it->y;
        if ((dx * dx) + (dy * dy) <= (it->radius * it->radius * 1.15f)) {
            return it->entity_id;
        }
    }
    return {};
}

} // namespace cad::picking
