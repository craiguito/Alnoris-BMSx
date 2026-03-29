#pragma once

#include "../battery/BatteryEntities.h"
#include "../core/CadDocument.h"
#include "../io/MeshLoader.h"
#include "ParametricPrimitives.h"

namespace cad::geometry {

class BatteryGeometryGenerator
{
public:
    GeometryBuffer buildVisualGeometry(
        const core::CadDocument& document,
        const battery::BatteryVisualizationOverlay& overlay,
        const io::TriangleMesh* cell_mesh
    ) const;
};

} // namespace cad::geometry
