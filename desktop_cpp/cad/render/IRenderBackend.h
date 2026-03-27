#pragma once

#include "RenderPacket.h"

namespace cad::render {

class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;
    virtual void render(const RenderPacket& packet) = 0;
};

} // namespace cad::render
