#pragma once

#include "../cad/core/CadDocument.h"

#include <QJsonArray>

class SimulationMappingBuilder
{
public:
    struct MappingResult
    {
        int groupCount = 1;
        QJsonArray thermalZones;
        QJsonArray groupZoneAssignments;
        QJsonArray groupLabels;
        QJsonArray groupEntityIds;
    };

    static MappingResult build(
        const cad::core::CadDocument& document,
        double defaultAmbientTempC,
        double defaultCoolingCoeffWPerK
    );
};
