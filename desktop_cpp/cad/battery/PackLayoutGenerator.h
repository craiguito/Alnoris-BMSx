#pragma once

#include "../core/CadDocument.h"
#include "BatteryConfig.h"

namespace cad::battery {

class PackLayoutGenerator
{
public:
    static void rebuildDocument(core::CadDocument& document, const PackLayoutConfig& config);
};

} // namespace cad::battery
