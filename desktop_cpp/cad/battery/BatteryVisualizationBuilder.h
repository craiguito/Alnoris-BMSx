#pragma once

#include "../core/CadDocument.h"
#include "BatteryConfig.h"
#include "BatteryEntities.h"

namespace cad::battery {

class BatteryVisualizationBuilder
{
public:
    static BatteryVisualizationOverlay build(
        const core::CadDocument& document,
        const ElectricalConfig& electrical,
        const ThermalConfig& thermal
    );
};

} // namespace cad::battery
