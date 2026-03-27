#pragma once

#include "../battery/BatteryEntities.h"
#include "../camera/Camera.h"
#include "../core/CadDocument.h"
#include "../io/MeshLoader.h"
#include "RenderPacket.h"

namespace cad::render {

class RenderComposer
{
public:
    RenderPacket compose(
        const core::CadDocument& document,
        const battery::BatteryVisualizationOverlay& overlay,
        const camera::Camera& camera,
        const io::TriangleMesh* cell_mesh,
        int viewport_width,
        int viewport_height
    ) const;
};

} // namespace cad::render
