#pragma once

#include "../core/EntityId.h"
#include "../render/RenderPacket.h"

namespace cad::picking {

class HitTester
{
public:
    [[nodiscard]] core::EntityId hitTestEntity(const render::RenderPacket& packet, float x, float y) const;
};

} // namespace cad::picking
