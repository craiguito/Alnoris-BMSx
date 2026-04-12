#include "SimulationMappingBuilder.h"

#include "../cad/battery/BatteryEntities.h"

#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>
#include <vector>

namespace {

struct GroupAggregate
{
    int seriesIndex = 0;
    QString label;
    cad::math::Vec3 centroid{};
    float averageHeight = 0.0f;
    QString entityId;
};

QString fallbackGroupLabel(int seriesIndex)
{
    return QString("Series Group %1").arg(seriesIndex + 1);
}

QString entityIdString(cad::core::EntityId entityId)
{
    return entityId.isValid() ? QString::number(static_cast<qulonglong>(entityId.value)) : QString();
}

bool isInsideBox(const cad::math::Vec3& point, const cad::math::Vec3& center, const cad::math::Vec3& size)
{
    return std::abs(point.x - center.x) <= size.x * 0.5f
        && std::abs(point.y - center.y) <= size.y * 0.5f
        && std::abs(point.z - center.z) <= size.z * 0.5f;
}

GroupAggregate makeAggregateFromCells(
    int seriesIndex,
    QString label,
    QString entityId,
    const cad::core::CadDocument& document,
    const std::vector<const cad::battery::CellEntity*>& cells)
{
    GroupAggregate aggregate;
    aggregate.seriesIndex = seriesIndex;
    aggregate.label = std::move(label);
    aggregate.entityId = std::move(entityId);

    if (cells.empty()) {
        return aggregate;
    }

    cad::math::Vec3 centroid{};
    float totalHeight = 0.0f;
    for (const cad::battery::CellEntity* cell : cells) {
        const cad::math::Vec3 worldPosition = document.worldPosition(cell->id);
        centroid.x += worldPosition.x;
        centroid.y += worldPosition.y;
        centroid.z += worldPosition.z;
        totalHeight += cell->height;
    }

    const float count = static_cast<float>(cells.size());
    aggregate.centroid = {
        centroid.x / count,
        centroid.y / count,
        centroid.z / count
    };
    aggregate.averageHeight = totalHeight / count;
    return aggregate;
}

std::vector<GroupAggregate> buildGroupAggregates(const cad::core::CadDocument& document)
{
    std::map<int, std::vector<const cad::battery::CellEntity*>> cellsBySeries;
    std::unordered_map<cad::core::EntityId, std::vector<const cad::battery::CellEntity*>, cad::core::EntityIdHash> cellsByParent;
    for (const cad::battery::CellEntity& cell : document.cells()) {
        cellsBySeries[cell.series_index].push_back(&cell);
        if (cell.parent_id.isValid()) {
            cellsByParent[cell.parent_id].push_back(&cell);
        }
    }

    if (!document.cellGroups().empty()) {
        std::vector<const cad::battery::CellGroupEntity*> orderedGroups;
        orderedGroups.reserve(document.cellGroups().size());
        for (const cad::battery::CellGroupEntity& group : document.cellGroups()) {
            orderedGroups.push_back(&group);
        }

        std::sort(orderedGroups.begin(), orderedGroups.end(), [](const auto* lhs, const auto* rhs) {
            if (lhs->series_index != rhs->series_index) {
                return lhs->series_index < rhs->series_index;
            }
            return lhs->id.value < rhs->id.value;
        });

        std::vector<GroupAggregate> aggregates;
        aggregates.reserve(orderedGroups.size());
        for (const cad::battery::CellGroupEntity* group : orderedGroups) {
            const auto parentIt = cellsByParent.find(group->id);
            const bool hasParentedCells = parentIt != cellsByParent.end() && !parentIt->second.empty();
            const auto seriesIt = cellsBySeries.find(group->series_index);
            const bool hasSeriesCells = seriesIt != cellsBySeries.end() && !seriesIt->second.empty();

            const std::vector<const cad::battery::CellEntity*>* cells = nullptr;
            if (hasParentedCells) {
                cells = &parentIt->second;
            } else if (hasSeriesCells) {
                cells = &seriesIt->second;
            }

            GroupAggregate aggregate = makeAggregateFromCells(
                group->series_index,
                group->label.empty() ? fallbackGroupLabel(group->series_index) : QString::fromStdString(group->label),
                entityIdString(group->id),
                document,
                cells != nullptr ? *cells : std::vector<const cad::battery::CellEntity*>{});

            if (cells == nullptr || cells->empty()) {
                aggregate.centroid = document.worldPosition(group->id);
                aggregate.averageHeight = group->size.y;
            }

            aggregates.push_back(std::move(aggregate));
        }
        return aggregates;
    }

    if (!cellsBySeries.empty()) {
        std::vector<GroupAggregate> aggregates;
        aggregates.reserve(cellsBySeries.size());
        for (const auto& [seriesIndex, cells] : cellsBySeries) {
            aggregates.push_back(makeAggregateFromCells(
                seriesIndex,
                fallbackGroupLabel(seriesIndex),
                QString("series-%1").arg(seriesIndex),
                document,
                cells));
        }
        return aggregates;
    }

    const int configuredGroupCount = std::max(1, document.metadata().layout_config.cells_in_series);
    std::vector<GroupAggregate> aggregates;
    aggregates.reserve(static_cast<std::size_t>(configuredGroupCount));
    for (int seriesIndex = 0; seriesIndex < configuredGroupCount; ++seriesIndex) {
        GroupAggregate aggregate;
        aggregate.seriesIndex = seriesIndex;
        aggregate.label = fallbackGroupLabel(seriesIndex);
        aggregate.entityId = QString("series-%1").arg(seriesIndex);
        aggregates.push_back(std::move(aggregate));
    }
    return aggregates;
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

    const std::vector<GroupAggregate> groups = buildGroupAggregates(document);
    result.groupCount = static_cast<int>(groups.size());

    std::vector<const cad::battery::CoolingPlateEntity*> coolingPlates;
    for (const cad::battery::CoolingPlateEntity& plate : document.coolingPlates()) {
        coolingPlates.push_back(&plate);
    }
    std::sort(coolingPlates.begin(), coolingPlates.end(), [](const auto* lhs, const auto* rhs) {
        if (lhs->plate_index != rhs->plate_index) {
            return lhs->plate_index < rhs->plate_index;
        }
        return lhs->id.value < rhs->id.value;
    });
    for (const cad::battery::CoolingPlateEntity* plate : coolingPlates) {
        result.thermalZones.append(QJsonObject{
            {"zone_id", 100 + plate->plate_index},
            {"name", QString::fromStdString(plate->label.empty() ? "Cooling Plate" : plate->label)},
            {"cooling_coeff_multiplier", 1.35},
            {"note", "Derived from cooling plate proximity."}
        });
    }

    std::vector<const cad::battery::ModuleBoundaryEntity*> moduleBoundaries;
    for (const cad::battery::ModuleBoundaryEntity& boundary : document.moduleBoundaries()) {
        moduleBoundaries.push_back(&boundary);
    }
    std::sort(moduleBoundaries.begin(), moduleBoundaries.end(), [](const auto* lhs, const auto* rhs) {
        if (lhs->module_index != rhs->module_index) {
            return lhs->module_index < rhs->module_index;
        }
        return lhs->id.value < rhs->id.value;
    });
    for (const cad::battery::ModuleBoundaryEntity* boundary : moduleBoundaries) {
        result.thermalZones.append(QJsonObject{
            {"zone_id", 200 + boundary->module_index},
            {"name", QString::fromStdString(boundary->label.empty() ? "Module Zone" : boundary->label)},
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
        result.groupLabels.append(group.label);
        result.groupEntityIds.append(group.entityId);
    }

    return result;
}
