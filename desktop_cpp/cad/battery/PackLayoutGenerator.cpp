#include "PackLayoutGenerator.h"

#include <algorithm>
#include <map>
#include <optional>
#include <tuple>

namespace cad::battery {
namespace {

using CellKey = std::pair<int, int>;

template <typename T>
void copyPersistentFields(T& target, const T& source)
{
    target.id = source.id;
    target.label = source.label;
    target.visible = source.visible;
}

} // namespace

void PackLayoutGenerator::rebuildDocument(core::CadDocument& document, const PackLayoutConfig& config)
{
    const core::EntityId previous_selection = document.selection().primary;

    std::map<CellKey, CellEntity> previousCells;
    for (const CellEntity& cell : document.cells()) {
        previousCells[{cell.series_index, cell.parallel_index}] = cell;
    }

    std::map<BusbarRole, BusbarEntity> previousBusbars;
    for (const BusbarEntity& busbar : document.busbars()) {
        previousBusbars[busbar.role] = busbar;
    }

    std::map<int, CoolingPlateEntity> previousCoolingPlates;
    for (const CoolingPlateEntity& plate : document.coolingPlates()) {
        previousCoolingPlates[plate.plate_index] = plate;
    }

    std::map<int, ModuleBoundaryEntity> previousModuleBoundaries;
    for (const ModuleBoundaryEntity& boundary : document.moduleBoundaries()) {
        previousModuleBoundaries[boundary.module_index] = boundary;
    }

    std::map<int, PackEnclosureEntity> previousEnclosures;
    for (const PackEnclosureEntity& enclosure : document.packEnclosures()) {
        previousEnclosures[enclosure.enclosure_index] = enclosure;
    }

    document.clear();
    document.metadata().layout_config = config;

    const int series_count = std::max(1, config.cells_in_series);
    const int parallel_count = std::max(1, config.cells_in_parallel);
    const float pack_width = std::max(240.0f, (series_count - 1) * config.x_spacing + config.cell_radius * 2.8f);
    const float pack_depth = std::max(180.0f, (parallel_count - 1) * config.z_spacing + config.cell_radius * 2.8f);

    for (int row = 0; row < parallel_count; ++row) {
        for (int col = 0; col < series_count; ++col) {
            CellEntity cell;
            cell.label = "Cell";
            cell.position = {
                (col - (series_count - 1) / 2.0f) * config.x_spacing,
                0.0f,
                (row - (parallel_count - 1) / 2.0f) * config.z_spacing
            };
            cell.radius = config.cell_radius;
            cell.height = config.cell_height;
            cell.series_index = col;
            cell.parallel_index = row;

            const auto previousIt = previousCells.find({col, row});
            if (previousIt != previousCells.end()) {
                copyPersistentFields(cell, previousIt->second);
            }

            document.addCell(cell);
        }
    }

    CoolingPlateEntity coolingPlate;
    coolingPlate.label = "Cooling plate";
    coolingPlate.plate_index = 0;
    coolingPlate.center = {0.0f, -128.0f, 0.0f};
    coolingPlate.size = {pack_width + 72.0f, 24.0f, pack_depth + 92.0f};
    if (const auto previousIt = previousCoolingPlates.find(coolingPlate.plate_index); previousIt != previousCoolingPlates.end()) {
        copyPersistentFields(coolingPlate, previousIt->second);
    }
    document.addCoolingPlate(coolingPlate);

    BusbarEntity negativeBusbar;
    negativeBusbar.label = "Negative busbar";
    negativeBusbar.role = BusbarRole::Negative;
    negativeBusbar.center = {0.0f, 112.0f, -pack_depth * 0.5f};
    negativeBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    if (const auto previousIt = previousBusbars.find(negativeBusbar.role); previousIt != previousBusbars.end()) {
        copyPersistentFields(negativeBusbar, previousIt->second);
    }
    document.addBusbar(negativeBusbar);

    BusbarEntity positiveBusbar;
    positiveBusbar.label = "Positive busbar";
    positiveBusbar.role = BusbarRole::Positive;
    positiveBusbar.center = {0.0f, 112.0f, pack_depth * 0.5f};
    positiveBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    if (const auto previousIt = previousBusbars.find(positiveBusbar.role); previousIt != previousBusbars.end()) {
        copyPersistentFields(positiveBusbar, previousIt->second);
    }
    document.addBusbar(positiveBusbar);

    ModuleBoundaryEntity moduleBoundary;
    moduleBoundary.label = "Module boundary";
    moduleBoundary.module_index = 0;
    moduleBoundary.center = {0.0f, 0.0f, 0.0f};
    moduleBoundary.size = {pack_width + 96.0f, config.cell_height + 56.0f, pack_depth + 92.0f};
    if (const auto previousIt = previousModuleBoundaries.find(moduleBoundary.module_index); previousIt != previousModuleBoundaries.end()) {
        copyPersistentFields(moduleBoundary, previousIt->second);
    }
    document.addModuleBoundary(moduleBoundary);

    PackEnclosureEntity enclosure;
    enclosure.label = "Pack enclosure";
    enclosure.enclosure_index = 0;
    enclosure.center = {0.0f, 0.0f, 0.0f};
    enclosure.size = {pack_width + 140.0f, config.cell_height + 92.0f, pack_depth + 136.0f};
    enclosure.wall_thickness = 8.0f;
    if (const auto previousIt = previousEnclosures.find(enclosure.enclosure_index); previousIt != previousEnclosures.end()) {
        copyPersistentFields(enclosure, previousIt->second);
    }
    document.addPackEnclosure(enclosure);

    if (document.hasEntity(previous_selection)) {
        document.selectEntity(previous_selection);
    } else {
        document.clearSelection();
    }
}

} // namespace cad::battery
