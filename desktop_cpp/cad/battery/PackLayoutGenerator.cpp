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

    if (existing.property_modes.label == PropertyMode::UserOverride) {
        generated.label = existing.label;
    }
    if (existing.property_modes.visibility == PropertyMode::UserOverride) {
        generated.visible = existing.visible;
    }
    generated.property_modes.label = existing.property_modes.label;
    generated.property_modes.visibility = existing.property_modes.visibility;
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
            CellEntity generated;
            generated.label = "Cell";
            generated.position = {
                (col - (series_count - 1) / 2.0f) * config.x_spacing,
                0.0f,
                (row - (parallel_count - 1) / 2.0f) * config.z_spacing
            };
            generated.radius = config.cell_radius;
            generated.height = config.cell_height;
            generated.series_index = col;
            generated.parallel_index = row;

            const auto previousIt = previousCells.find({col, row});
            const CellEntity merged = mergeCell(generated, previousIt == previousCells.end() ? nullptr : &previousIt->second);
            document.addCell(merged);
        }
    }

    CoolingPlateEntity generatedCoolingPlate;
    generatedCoolingPlate.label = "Cooling plate";
    generatedCoolingPlate.plate_index = 0;
    generatedCoolingPlate.center = {0.0f, -128.0f, 0.0f};
    generatedCoolingPlate.size = {pack_width + 72.0f, 24.0f, pack_depth + 92.0f};
    const auto previousPlate = previousCoolingPlates.find(generatedCoolingPlate.plate_index);
    document.addCoolingPlate(mergeCoolingPlate(generatedCoolingPlate, previousPlate == previousCoolingPlates.end() ? nullptr : &previousPlate->second));

    BusbarEntity generatedNegativeBusbar;
    generatedNegativeBusbar.label = "Negative busbar";
    generatedNegativeBusbar.role = BusbarRole::Negative;
    generatedNegativeBusbar.center = {0.0f, 112.0f, -pack_depth * 0.5f};
    generatedNegativeBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    const auto previousNegativeBusbar = previousBusbars.find(generatedNegativeBusbar.role);
    document.addBusbar(mergeBusbar(generatedNegativeBusbar, previousNegativeBusbar == previousBusbars.end() ? nullptr : &previousNegativeBusbar->second));

    BusbarEntity generatedPositiveBusbar;
    generatedPositiveBusbar.label = "Positive busbar";
    generatedPositiveBusbar.role = BusbarRole::Positive;
    generatedPositiveBusbar.center = {0.0f, 112.0f, pack_depth * 0.5f};
    generatedPositiveBusbar.size = {pack_width + 96.0f, 12.0f, 16.0f};
    const auto previousPositiveBusbar = previousBusbars.find(generatedPositiveBusbar.role);
    document.addBusbar(mergeBusbar(generatedPositiveBusbar, previousPositiveBusbar == previousBusbars.end() ? nullptr : &previousPositiveBusbar->second));

    ModuleBoundaryEntity generatedModuleBoundary;
    generatedModuleBoundary.label = "Module boundary";
    generatedModuleBoundary.module_index = 0;
    generatedModuleBoundary.center = {0.0f, 0.0f, 0.0f};
    generatedModuleBoundary.size = {pack_width + 96.0f, config.cell_height + 56.0f, pack_depth + 92.0f};
    const auto previousModuleBoundary = previousModuleBoundaries.find(generatedModuleBoundary.module_index);
    document.addModuleBoundary(mergeModuleBoundary(generatedModuleBoundary, previousModuleBoundary == previousModuleBoundaries.end() ? nullptr : &previousModuleBoundary->second));

    PackEnclosureEntity generatedEnclosure;
    generatedEnclosure.label = "Pack enclosure";
    generatedEnclosure.enclosure_index = 0;
    generatedEnclosure.center = {0.0f, 0.0f, 0.0f};
    generatedEnclosure.size = {pack_width + 140.0f, config.cell_height + 92.0f, pack_depth + 136.0f};
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
