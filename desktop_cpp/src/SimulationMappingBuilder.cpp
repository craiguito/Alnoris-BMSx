#include "SimulationMappingBuilder.h"

#include "../cad/battery/BatteryEntities.h"

#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <vector>

namespace {

struct GroupAggregate
{
    int seriesIndex = 0;
    cad::math::Vec3 centroid{};
    float averageHeight = 0.0f;
    QString entityId;
};

bool isInsideBox(const cad::math::Vec3& point, const cad::math::Vec3& center, const cad::math::Vec3& size)
{
    return std::abs(point.x - center.x) <= size.x * 0.5f
        && std::abs(point.y - center.y) <= size.y * 0.5f
        && std::abs(point.z - center.z) <= size.z * 0.5f;
}

} // namespace

SimulationMappingBuilder::MappingResult SimulationMappingBuilder::build(
    const cad::core::CadDocument& document,
    double defaultAmbientTempC,
    double defaultCoolingCoeffWPerK
)
{
    MappingResult result;
    result.thermalZones.append(QJsonObject{
        {"zone_id", 0},
        {"name", "Default Zone"},
        {"ambient_temp_c", defaultAmbientTempC},
        {"cooling_coeff_w_per_k", defaultCoolingCoeffWPerK},
        {"note", "Fallback lumped thermal behavior."}
    });

    const int configuredGroupCount = std::max(1, document.metadata().layout_config.cells_in_series);
    result.groupCount = configuredGroupCount;

    std::map<int, std::vector<const cad::battery::CellEntity*>> cellsBySeries;
    for (const cad::battery::CellEntity& cell : document.cells()) {
        if (!cell.visible) {
            continue;
        }
        cellsBySeries[cell.series_index].push_back(&cell);
    }

    std::vector<GroupAggregate> groups(static_cast<std::size_t>(configuredGroupCount));
    for (int seriesIndex = 0; seriesIndex < configuredGroupCount; ++seriesIndex) {
        groups[static_cast<std::size_t>(seriesIndex)].seriesIndex = seriesIndex;
        groups[static_cast<std::size_t>(seriesIndex)].entityId = QString("series-%1").arg(seriesIndex);

        const auto groupIt = std::find_if(
            document.cellGroups().begin(),
            document.cellGroups().end(),
            [seriesIndex](const cad::battery::CellGroupEntity& group) {
                return group.series_index == seriesIndex;
            }
        );
        if (groupIt != document.cellGroups().end()) {
            groups[static_cast<std::size_t>(seriesIndex)].entityId = QString::number(
                static_cast<qulonglong>(groupIt->id.value)
            );
        }

        const auto found = cellsBySeries.find(seriesIndex);
        if (found == cellsBySeries.end() || found->second.empty()) {
            continue;
        }

        cad::math::Vec3 centroid{};
        float totalHeight = 0.0f;
        for (const cad::battery::CellEntity* cell : found->second) {
            const cad::math::Vec3 worldPosition = document.worldPosition(cell->id);
            centroid.x += worldPosition.x;
            centroid.y += worldPosition.y;
            centroid.z += worldPosition.z;
            totalHeight += cell->height;
        }
        const float count = static_cast<float>(found->second.size());
        centroid.x /= count;
        centroid.y /= count;
        centroid.z /= count;
        groups[static_cast<std::size_t>(seriesIndex)].centroid = centroid;
        groups[static_cast<std::size_t>(seriesIndex)].averageHeight = totalHeight / count;
    }

    std::vector<const cad::battery::CoolingPlateEntity*> coolingPlates;
    for (const cad::battery::CoolingPlateEntity& plate : document.coolingPlates()) {
        if (!plate.visible) {
            continue;
        }
        coolingPlates.push_back(&plate);
        result.thermalZones.append(QJsonObject{
            {"zone_id", 100 + plate.plate_index},
            {"name", QString::fromStdString(plate.label.empty() ? "Cooling Plate" : plate.label)},
            {"cooling_coeff_multiplier", 1.35},
            {"note", "Derived from cooling plate proximity."}
        });
    }

    std::vector<const cad::battery::ModuleBoundaryEntity*> moduleBoundaries;
    for (const cad::battery::ModuleBoundaryEntity& boundary : document.moduleBoundaries()) {
        if (!boundary.visible) {
            continue;
        }
        moduleBoundaries.push_back(&boundary);
        result.thermalZones.append(QJsonObject{
            {"zone_id", 200 + boundary.module_index},
            {"name", QString::fromStdString(boundary.label.empty() ? "Module Zone" : boundary.label)},
            {"cooling_coeff_multiplier", 0.9},
            {"note", "Derived from module boundary containment."}
        });
    }

    for (const GroupAggregate& group : groups) {
        int assignedZoneId = 0;
        float bestCoolingDistance = std::numeric_limits<float>::max();
        for (const cad::battery::CoolingPlateEntity* plate : coolingPlates) {
            const cad::math::Vec3 plateCenter = document.worldPosition(plate->id);
            const bool withinPlanarFootprint =
                std::abs(group.centroid.x - plateCenter.x) <= plate->size.x * 0.5f &&
                std::abs(group.centroid.z - plateCenter.z) <= plate->size.z * 0.5f;
            const float verticalDistance = std::abs(group.centroid.y - plateCenter.y)
                - (plate->size.y * 0.5f + group.averageHeight * 0.5f);
            if (withinPlanarFootprint && verticalDistance <= 80.0f && verticalDistance < bestCoolingDistance) {
                bestCoolingDistance = verticalDistance;
                assignedZoneId = 100 + plate->plate_index;
            }
        }

        if (assignedZoneId == 0) {
            for (const cad::battery::ModuleBoundaryEntity* boundary : moduleBoundaries) {
                if (isInsideBox(group.centroid, document.worldPosition(boundary->id), boundary->size)) {
                    assignedZoneId = 200 + boundary->module_index;
                    break;
                }
            }
        }

        result.groupZoneAssignments.append(assignedZoneId);
        result.groupLabels.append(QString("Series Group %1").arg(group.seriesIndex + 1));
        result.groupEntityIds.append(group.entityId);
    }

    return result;
}
