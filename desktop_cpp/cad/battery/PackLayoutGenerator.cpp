#include "PackLayoutGenerator.h"
#include "PackLayoutMetrics.h"

#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

namespace cad::battery {
namespace {

using CellKey = std::pair<int, int>;
using BusbarKey = std::pair<int, BusbarRole>;

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

    std::map<BusbarKey, BusbarEntity> previousBusbars;
    for (const BusbarEntity& busbar : document.busbars()) {
        int moduleIndex = 0;
        if (const auto* parent = document.findModuleBoundary(busbar.parent_id); parent != nullptr) {
            moduleIndex = parent->module_index;
        }
        previousBusbars[{moduleIndex, busbar.role}] = busbar;
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

    const ResolvedPackLayout resolved = resolvePackLayout(config);
    const int series_count = resolved.series_count;
    const int parallel_count = resolved.parallel_count;
    const bool cylindrical = resolved.cylindrical;

    BatteryPackEntity generatedPack;
    generatedPack.label = "Battery pack";
    generatedPack.center = {0.0f, resolved.stack.bounds_center_y, 0.0f};
    generatedPack.size = {resolved.pack_outer_width, resolved.stack.bounds_height, resolved.pack_outer_depth};
    generatedPack.series_count = series_count;
    generatedPack.parallel_count = parallel_count;
    generatedPack.layout_type = LayoutType::Grid;
    generatedPack.cell_radius = config.cell_radius;
    generatedPack.cell_height = config.cell_height;
    generatedPack.spacing_x = resolved.pitch_x;
    generatedPack.spacing_z = resolved.pitch_z;
    const BatteryPackEntity mergedPack = mergePack(generatedPack, previousPack);
    BatteryPackEntity& pack = document.addPack(mergedPack);

    for (const ModuleLayoutSlice& slice : resolved.modules) {
        const int moduleIndex = slice.module_index;

        ModuleBoundaryEntity generatedModuleBoundary;
        generatedModuleBoundary.label = resolved.module_count == 1
            ? "Battery module"
            : ("Battery module " + std::to_string(moduleIndex + 1));
        generatedModuleBoundary.parent_id = pack.id;
        generatedModuleBoundary.module_index = moduleIndex;
        generatedModuleBoundary.center = {slice.center_x, 0.0f, 0.0f};
        generatedModuleBoundary.size = {slice.boundary_width, resolved.stack.bounds_height, resolved.boundary_depth};
        generatedModuleBoundary.series_span = slice.series_count;
        generatedModuleBoundary.parallel_span = parallel_count;
        const auto previousModuleBoundary = previousModuleBoundaries.find(moduleIndex);
        ModuleBoundaryEntity& module = document.addModuleBoundary(
            mergeModuleBoundary(generatedModuleBoundary, previousModuleBoundary == previousModuleBoundaries.end() ? nullptr : &previousModuleBoundary->second)
        );
        for (int localSeries = 0; localSeries < slice.series_count; ++localSeries) {
            const int globalSeries = slice.series_start + localSeries;
            const float localCenterX = (localSeries - (slice.series_count - 1) / 2.0f) * resolved.pitch_x;

            CellGroupEntity generatedGroup;
            generatedGroup.label = "Cell group";
            generatedGroup.parent_id = module.id;
            generatedGroup.group_index = globalSeries;
            generatedGroup.series_index = globalSeries;
            generatedGroup.simulation_group_index = globalSeries;
            generatedGroup.center = {localCenterX, 0.0f, 0.0f};
            generatedGroup.size = {resolved.group_width, resolved.stack.bounds_height, resolved.tray_depth};
            generatedGroup.cell_count = parallel_count;
            const auto previousGroup = previousGroups.find(globalSeries);
            document.addCellGroup(mergeGroup(generatedGroup, previousGroup == previousGroups.end() ? nullptr : &previousGroup->second));
        }

        CoolingPlateEntity generatedCoolingPlate;
        generatedCoolingPlate.label = resolved.module_count == 1
            ? "Cooling plate"
            : ("Cooling plate " + std::to_string(moduleIndex + 1));
        generatedCoolingPlate.parent_id = module.id;
        generatedCoolingPlate.plate_index = moduleIndex;
        generatedCoolingPlate.center = {
            0.0f,
            resolved.stack.cooling_plate_center_y - resolved.stack.bounds_center_y,
            0.0f
        };
        generatedCoolingPlate.size = {slice.cooling_width, config.cooling_channel_thickness, resolved.cooling_depth};
        const auto previousPlate = previousCoolingPlates.find(generatedCoolingPlate.plate_index);
        document.addCoolingPlate(mergeCoolingPlate(generatedCoolingPlate, previousPlate == previousCoolingPlates.end() ? nullptr : &previousPlate->second));

        BusbarEntity generatedNegativeBusbar;
        generatedNegativeBusbar.label = "Negative busbar";
        generatedNegativeBusbar.parent_id = module.id;
        generatedNegativeBusbar.role = BusbarRole::Negative;
        generatedNegativeBusbar.center = {
            0.0f,
            resolved.stack.busbar_center_y - resolved.stack.bounds_center_y,
            -resolved.row_center_z
        };
        generatedNegativeBusbar.size = {slice.busbar_length, config.busbar_thickness, config.busbar_width};
        const auto previousNegativeBusbar = previousBusbars.find({moduleIndex, generatedNegativeBusbar.role});
        document.addBusbar(mergeBusbar(generatedNegativeBusbar, previousNegativeBusbar == previousBusbars.end() ? nullptr : &previousNegativeBusbar->second));

        BusbarEntity generatedPositiveBusbar;
        generatedPositiveBusbar.label = "Positive busbar";
        generatedPositiveBusbar.parent_id = module.id;
        generatedPositiveBusbar.role = BusbarRole::Positive;
        generatedPositiveBusbar.center = {
            0.0f,
            resolved.stack.busbar_center_y - resolved.stack.bounds_center_y,
            resolved.row_center_z
        };
        generatedPositiveBusbar.size = {slice.busbar_length, config.busbar_thickness, config.busbar_width};
        const auto previousPositiveBusbar = previousBusbars.find({moduleIndex, generatedPositiveBusbar.role});
        document.addBusbar(mergeBusbar(generatedPositiveBusbar, previousPositiveBusbar == previousBusbars.end() ? nullptr : &previousPositiveBusbar->second));
    }

    for (int row = 0; row < parallel_count; ++row) {
        for (int col = 0; col < series_count; ++col) {
            CellEntity generated;
            generated.label = "Battery cell";
            generated.parent_id = document.cellGroups()[static_cast<std::size_t>(col)].id;
            generated.position = {
                0.0f,
                resolved.stack.cell_center_y - resolved.stack.bounds_center_y,
                (row - (parallel_count - 1) / 2.0f) * resolved.pitch_z
            };
            generated.form_factor = config.cell_form_factor;
            generated.radius = config.cell_radius;
            generated.height = config.cell_height;
            generated.width = resolved.cell_width;
            generated.depth = resolved.cell_depth;
            generated.series_index = col;
            generated.parallel_index = row;
            generated.simulation_group_index = col;
            generated.cell_type = config.cell_form_factor == CellFormFactor::Prismatic
                ? "prismatic"
                : (config.cell_form_factor == CellFormFactor::Pouch ? "pouch"
                    : (resolved.cell_diameter >= 20.0f && config.cell_height >= 68.0f ? "21700" : "18650"));

            const auto previousIt = previousCells.find({col, row});
            const CellEntity merged = mergeCell(generated, previousIt == previousCells.end() ? nullptr : &previousIt->second);
            document.addCell(merged);
        }
    }

    PackEnclosureEntity generatedEnclosure;
    generatedEnclosure.label = "Enclosure";
    generatedEnclosure.parent_id = pack.id;
    generatedEnclosure.enclosure_index = 0;
    generatedEnclosure.center = {0.0f, 0.0f, 0.0f};
    generatedEnclosure.size = {resolved.pack_outer_width, resolved.stack.bounds_height, resolved.pack_outer_depth};
    generatedEnclosure.wall_thickness = config.enclosure_wall_thickness;
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
