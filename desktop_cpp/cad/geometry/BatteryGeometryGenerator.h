#pragma once

#include "../battery/BatteryEntities.h"
#include "../core/CadDocument.h"
#include "../io/MeshLoader.h"
#include "ParametricPrimitives.h"

#include <string>
#include <unordered_map>

namespace cad::geometry {

class BatteryGeometryGenerator
{
public:
    GeometryBuffer buildVisualGeometry(
        const core::CadDocument& document,
        const battery::BatteryVisualizationOverlay& overlay,
        const io::TriangleMesh* cell_mesh
    ) const;

private:
    GeometryBuffer buildCachedCylindricalCellGeometry(
        const CylindricalCellProfile& profile,
        int segments,
        const math::Vec3& body_color,
        const math::Vec3& cap_color,
        const math::Vec3& insulator_color
    ) const;

    [[nodiscard]] const GeometryBuffer& cachedCylindricalCellGeometry(
        const CylindricalCellProfile& profile,
        int segments,
        const math::Vec3& body_color,
        const math::Vec3& cap_color,
        const math::Vec3& insulator_color
    ) const;

    mutable std::unordered_map<std::string, GeometryBuffer> m_cylindricalCellCache;
};

} // namespace cad::geometry
