#include "PackLayoutGenerator.h"

#include <algorithm>
#include <map>
#include <utility>

namespace cad::battery {
namespace {

using CellKey = std::pair<int, int>;

template <typename T>
void preserveCommonOverrides(T& generated, const T& existing)
{
    generated.id = existing.id;
    generated.parent_id = existing.parent_id;

    if (existing.property_modes.label == PropertyMode::UserOverride) {
        generated.label = existing.label;
    }
    if (existing.property_modes.visibility == PropertyMode::UserOverride) {
        generated.visible = existing.visible;
    }
    generated.property_modes.label = existing.property_modes.label;
    generated.property_modes.visibility = existing.property_modes.visibility;
}

BatteryPackEntity mergePack(const BatteryPackEntity& generated, const BatteryPackEntity* existing)
{
    if (existing == nullptr) {
        return generated;
    }

    BatteryPackEntity merged = generated;
    preserveCommonOverrides(merged, *existing);
    if (existing->property_modes.position == PropertyMode::UserOverride) {
        merged.center = existing->center;
    }
    if (existing->property_modes.geometry == PropertyMode::UserOverride) {
        merged.size = existing->size;
    }
    merged.property_modes.position = existing->property_modes.position;
    merged.property_modes.geometry = existing->property_modes.geometry;
    return merged;
}

CellGroupEntity mergeGroup(const CellGroupEntity& generated, const CellGroupEntity* existing)
{
    if (existing == nullptr) {
        return generated;
    }

    CellGroupEntity merged = generated;
    preserveCommonOverrides(merged, *existing);
    if (existing->property_modes.position == PropertyMode::UserOverride) {
        merged.center = existing->center;
    }
    if (existing->property_modes.geometry == PropertyMode::UserOverride) {
        merged.size = existing->size;
    }
    merged.property_modes.position = existing->property_modes.position;
    merged.property_modes.geometry = existing->property_modes.geometry;
    return merged;
}

CellEntity mergeCell(const CellEntity& generated, const CellEntity* existing)
{
    if (existing == nullptr) {
        return generated;
    }

    CellEntity merged = generated;
    preserveCommonOverrides(merged, *existing);
    if (existing->property_modes.position == PropertyMode::UserOverride) {
        merged.position = existing->position;
    }
    if (existing->property_modes.geometry == PropertyMode::UserOverride) {
        merged.radius = existing->radius;
        merged.height = existing->height;
    }
    merged.property_modes.position = existing->property_modes.position;
    merged.property_modes.geometry = existing->property_modes.geometry;
    return merged;
}

BusbarEntity mergeBusbar(const BusbarEntity& generated, const BusbarEntity* existing)
{
    if (existing == nullptr) {
        return generated;
    }

    BusbarEntity merged = generated;
    preserveCommonOverrides(merged, *existing);
    if (existing->property_modes.position == PropertyMode::UserOverride) {
        merged.center = existing->center;
    }
    if (existing->property_modes.geometry == PropertyMode::UserOverride) {
        merged.size = existing->size;
    }
    merged.property_modes.position = existing->property_modes.position;
    merged.property_modes.geometry = existing->property_modes.geometry;
    return merged;
}

CoolingPlateEntity mergeCoolingPlate(const CoolingPlateEntity& generated, const CoolingPlateEntity* existing)
{
    if (existing == nullptr) {
        return generated;
    }

    CoolingPlateEntity merged = generated;
    preserveCommonOverrides(merged, *existing);
    if (existing->property_modes.position == PropertyMode::UserOverride) {
        merged.center = existing->center;
    }
    if (existing->property_modes.geometry == PropertyMode::UserOverride) {
        merged.size = existing->size;
    }
    merged.property_modes.position = existing->property_modes.position;
    merged.property_modes.geometry = existing->property_modes.geometry;
    return merged;
}

ModuleBoundaryEntity mergeModuleBoundary(const ModuleBoundaryEntity& generated, const ModuleBoundaryEntity* existing)
{
    if (existing == nullptr) {
        return generated;
    }

    ModuleBoundaryEntity merged = generated;
    preserveCommonOverrides(merged, *existing);
    if (existing->property_modes.position == PropertyMode::UserOverride) {
        merged.center = existing->center;
    }
    if (existing->property_modes.geometry == PropertyMode::UserOverride) {
        merged.size = existing->size;
    }
    merged.property_modes.position = existing->property_modes.position;
    merged.property_modes.geometry = existing->property_modes.geometry;
    return merged;
}

PackEnclosureEntity mergeEnclosure(const PackEnclosureEntity& generated, const PackEnclosureEntity* existing)
{
    if (existing == nullptr) {
        return generated;
    }

    PackEnclosureEntity merged = generated;
    preserveCommonOverrides(merged, *existing);
    if (existing->property_modes.position == PropertyMode::UserOverride) {
        merged.center = existing->center;
    }
    if (existing->property_modes.geometry == PropertyMode::UserOverride) {
        merged.size = existing->size;
        merged.wall_thickness = existing->wall_thickness;
    }
    merged.property_modes.position = existing->property_modes.position;
    merged.property_modes.geometry = existing->property_modes.geometry;
    return merged;
}

} // namespace

void PackLayoutGenerator::rebuildDocument(core::CadDocument& document, const PackLayoutConfig& config)
{
    const core::EntityId previous_selection = document.selection().primary;
    const BatteryPackEntity* previousPack = document.packs().empty() ? nullptr : &document.packs().front();

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

    std::map<int, CellGroupEntity> previousGroups;
    for (const CellGroupEntity& group : document.cellGroups()) {
        previousGroups[group.series_index] = group;
    }

    document.clear();
    document.metadata().layout_config = config;

    const int series_count = std::max(1, config.cells_in_series);
    const int parallel_count = std::max(1, config.cells_in_parallel);
    const float pack_width = std::max(240.0f, (series_count - 1) * config.x_spacing + config.cell_radius * 2.8f);
    const float pack_depth = std::max(180.0f, (parallel_count - 1) * config.z_spacing + config.cell_radius * 2.8f);
    const float module_height = config.cell_height + 56.0f;
    const float enclosure_height = config.cell_height + 92.0f;
    const float group_width = std::max(config.x_spacing * 0.78f, config.cell_radius * 2.8f);
    const float group_depth = std::max(pack_depth + 28.0f, config.cell_radius * 2.8f);

    BatteryPackEntity generatedPack;
    generatedPack.label = "Battery pack";
    generatedPack.center = {0.0f, 0.0f, 0.0f};
    generatedPack.size = {pack_width + 140.0f, enclosure_height, pack_depth + 136.0f};
    generatedPack.series_count = series_count;
    generatedPack.parallel_count = parallel_count;
    generatedPack.cell_radius = config.cell_radius;
    generatedPack.cell_height = config.cell_height;
    generatedPack.spacing_x = config.x_spacing;
    generatedPack.spacing_z = config.z_spacing;
    const BatteryPackEntity mergedPack = mergePack(generatedPack, previousPack);
    BatteryPackEntity& pack = document.addPack(mergedPack);

    ModuleBoundaryEntity generatedModuleBoundary;
    generatedModuleBoundary.label = "Battery module";
    generatedModuleBoundary.parent_id = pack.id;
    generatedModuleBoundary.module_index = 0;
    generatedModuleBoundary.center = {0.0f, 0.0f, 0.0f};
    generatedModuleBoundary.size = {pack_width + 96.0f, module_height, pack_depth + 92.0f};
    generatedModuleBoundary.series_span = series_count;
    generatedModuleBoundary.parallel_span = parallel_count;
    const auto previousModuleBoundary = previousModuleBoundaries.find(generatedModuleBoundary.module_index);
    ModuleBoundaryEntity& module = document.addModuleBoundary(
        mergeModuleBoundary(generatedModuleBoundary, previousModuleBoundary == previousModuleBoundaries.end() ? nullptr : &previousModuleBoundary->second)
    );

    for (int col = 0; col < series_count; ++col) {
        CellGroupEntity generatedGroup;
        generatedGroup.label = "Cell group";
        generatedGroup.parent_id = module.id;
        generatedGroup.group_index = col;
        generatedGroup.series_index = col;
        generatedGroup.simulation_group_index = col;
        generatedGroup.center = {
            (col - (series_count - 1) / 2.0f) * config.x_spacing,
            0.0f,
            0.0f
        };
        generatedGroup.size = {group_width, config.cell_height + 24.0f, group_depth};
        generatedGroup.cell_count = parallel_count;
        const auto previousGroup = previousGroups.find(col);
        document.addCellGroup(mergeGroup(generatedGroup, previousGroup == previousGroups.end() ? nullptr : &previousGroup->second));
    }

    for (int row = 0; row < parallel_count; ++row) {
        for (int col = 0; col < series_count; ++col) {
            CellEntity generated;
            generated.label = "Battery cell";
            generated.parent_id = document.cellGroups()[static_cast<std::size_t>(col)].id;
            generated.position = {
                0.0f,
                0.0f,
                (row - (parallel_count - 1) / 2.0f) * config.z_spacing
            };
            generated.radius = config.cell_radius;
            generated.height = config.cell_height;
            generated.series_index = col;
            generated.parallel_index = row;
            generated.simulation_group_index = col;
            generated.cell_type = config.cell_radius >= 32.0f ? "21700" : "18650";

            const auto previousIt = previousCells.find({col, row});
            const CellEntity merged = mergeCell(generated, previousIt == previousCells.end() ? nullptr : &previousIt->second);
            document.addCell(merged);
        }
    }

    CoolingPlateEntity generatedCoolingPlate;
    generatedCoolingPlate.label = "Cooling channel";
    generatedCoolingPlate.parent_id = module.id;
    generatedCoolingPlate.plate_index = 0;
    generatedCoolingPlate.center = {0.0f, -128.0f, 0.0f};
    generatedCoolingPlate.size = {pack_width + 72.0f, 24.0f, pack_depth + 92.0f};
    const auto previousPlate = previousCoolingPlates.find(generatedCoolingPlate.plate_index);
    document.addCoolingPlate(mergeCoolingPlate(generatedCoolingPlate, previousPlate == previousCoolingPlates.end() ? nullptr : &previousPlate->second));

    BusbarEntity generatedNegativeBusbar;
    generatedNegativeBusbar.label = "Negative busbar";
    generatedNegativeBusbar.parent_id = module.id;
    generatedNegativeBusbar.role = BusbarRole::Negative;
    generatedNegativeBusbar.center = {0.0f, 112.0f, -pack_depth * 0.5f};
    generatedNegativeBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    const auto previousNegativeBusbar = previousBusbars.find(generatedNegativeBusbar.role);
    document.addBusbar(mergeBusbar(generatedNegativeBusbar, previousNegativeBusbar == previousBusbars.end() ? nullptr : &previousNegativeBusbar->second));

    BusbarEntity generatedPositiveBusbar;
    generatedPositiveBusbar.label = "Positive busbar";
    generatedPositiveBusbar.parent_id = module.id;
    generatedPositiveBusbar.role = BusbarRole::Positive;
    generatedPositiveBusbar.center = {0.0f, 112.0f, pack_depth * 0.5f};
    generatedPositiveBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    const auto previousPositiveBusbar = previousBusbars.find(generatedPositiveBusbar.role);
    document.addBusbar(mergeBusbar(generatedPositiveBusbar, previousPositiveBusbar == previousBusbars.end() ? nullptr : &previousPositiveBusbar->second));

    PackEnclosureEntity generatedEnclosure;
    generatedEnclosure.label = "Enclosure";
    generatedEnclosure.parent_id = pack.id;
    generatedEnclosure.enclosure_index = 0;
    generatedEnclosure.center = {0.0f, 0.0f, 0.0f};
    generatedEnclosure.size = {pack_width + 140.0f, enclosure_height, pack_depth + 136.0f};
    generatedEnclosure.wall_thickness = 8.0f;
    const auto previousEnclosure = previousEnclosures.find(generatedEnclosure.enclosure_index);
    document.addPackEnclosure(mergeEnclosure(generatedEnclosure, previousEnclosure == previousEnclosures.end() ? nullptr : &previousEnclosure->second));

    if (document.hasEntity(previous_selection)) {
        document.selectEntity(previous_selection);
    } else {
        document.clearSelection();
    }

    // TODO: if future layout diffs become more complex, consider a richer merge strategy than logical-key replacement.
}

} // namespace cad::battery
