#include "CadEditService.h"

#include "../battery/PackLayoutMetrics.h"

#include <algorithm>

namespace cad::edit {
namespace {

battery::ResolvedPackLayout resolvedLayout(const battery::PackLayoutConfig& config)
{
    return battery::resolvePackLayout(config);
}

battery::ModuleLayoutSlice moduleSliceForSeries(const battery::ResolvedPackLayout& resolved, int series_index)
{
    for (const battery::ModuleLayoutSlice& slice : resolved.modules) {
        if (series_index >= slice.series_start && series_index < slice.series_start + slice.series_count) {
            return slice;
        }
    }
    return resolved.modules.empty() ? battery::ModuleLayoutSlice{} : resolved.modules.front();
}

math::Vec3 generatedCellPosition(const battery::ResolvedPackLayout& resolved, const battery::PackLayoutConfig& config, int parallel_index)
{
    (void)config;
    return {
        0.0f,
        resolved.stack.cell_center_y - resolved.stack.bounds_center_y,
        (parallel_index - (resolved.parallel_count - 1) / 2.0f) * resolved.pitch_z
    };
}

math::Vec3 generatedGroupPosition(const battery::ResolvedPackLayout& resolved, const battery::PackLayoutConfig& config, int series_index)
{
    const battery::ModuleLayoutSlice slice = moduleSliceForSeries(resolved, series_index);
    const int local_series = series_index - slice.series_start;
    return {
        (local_series - (slice.series_count - 1) / 2.0f) * resolved.pitch_x,
        0.0f,
        0.0f
    };
}

} // namespace

bool CadEditService::moveEntity(core::CadDocument& document, core::EntityId entity_id, const math::Vec3& delta)
{
    return document.moveEntity(entity_id, delta);
}

bool CadEditService::setEntityLabel(core::CadDocument& document, core::EntityId entity_id, std::string label)
{
    return document.setEntityLabel(entity_id, std::move(label));
}

bool CadEditService::setEntityVisibility(core::CadDocument& document, core::EntityId entity_id, bool visible)
{
    return document.setEntityVisibility(entity_id, visible);
}

bool CadEditService::updateCellProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CellPropertiesUpdate& update)
{
    return document.updateCellProperties(entity_id, update);
}

bool CadEditService::updateBusbarProperties(core::CadDocument& document, core::EntityId entity_id, const battery::BusbarPropertiesUpdate& update)
{
    return document.updateBusbarProperties(entity_id, update);
}

bool CadEditService::updateCoolingPlateProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CoolingPlatePropertiesUpdate& update)
{
    return document.updateCoolingPlateProperties(entity_id, update);
}

bool CadEditService::updateModuleBoundaryProperties(core::CadDocument& document, core::EntityId entity_id, const battery::ModuleBoundaryPropertiesUpdate& update)
{
    return document.updateModuleBoundaryProperties(entity_id, update);
}

bool CadEditService::updateEnclosureProperties(core::CadDocument& document, core::EntityId entity_id, const battery::PackEnclosurePropertiesUpdate& update)
{
    return document.updateEnclosureProperties(entity_id, update);
}

bool CadEditService::applyCellProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CellProperties& properties)
{
    return document.applyCellProperties(entity_id, properties);
}

bool CadEditService::applyBusbarProperties(core::CadDocument& document, core::EntityId entity_id, const battery::BusbarProperties& properties)
{
    return document.applyBusbarProperties(entity_id, properties);
}

bool CadEditService::applyCoolingPlateProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CoolingPlateProperties& properties)
{
    return document.applyCoolingPlateProperties(entity_id, properties);
}

bool CadEditService::applyModuleBoundaryProperties(core::CadDocument& document, core::EntityId entity_id, const battery::ModuleBoundaryProperties& properties)
{
    return document.applyModuleBoundaryProperties(entity_id, properties);
}

bool CadEditService::applyEnclosureProperties(core::CadDocument& document, core::EntityId entity_id, const battery::PackEnclosureProperties& properties)
{
    return document.applyEnclosureProperties(entity_id, properties);
}

bool CadEditService::resetEntityPositionToGenerated(core::CadDocument& document, const battery::PackLayoutConfig& layout, core::EntityId entity_id)
{
    const battery::ResolvedPackLayout resolved = resolvedLayout(layout);
    if (battery::BatteryPackEntity* pack = document.findPack(entity_id)) {
        pack->center = {0.0f, resolved.stack.bounds_center_y, 0.0f};
        pack->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = document.findCellGroup(entity_id)) {
        group->center = generatedGroupPosition(resolved, layout, group->series_index);
        group->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        cell->position = generatedCellPosition(resolved, layout, cell->parallel_index);
        cell->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        busbar->center = {
            0.0f,
            resolved.stack.busbar_center_y - resolved.stack.bounds_center_y,
            busbar->role == battery::BusbarRole::Negative ? -resolved.row_center_z : resolved.row_center_z
        };
        busbar->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        plate->center = {0.0f, resolved.stack.cooling_plate_center_y - resolved.stack.bounds_center_y, 0.0f};
        plate->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        boundary->center = {0.0f, 0.0f, 0.0f};
        boundary->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        enclosure->center = {0.0f, 0.0f, 0.0f};
        enclosure->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadEditService::resetEntityGeometryToGenerated(core::CadDocument& document, const battery::PackLayoutConfig& layout, core::EntityId entity_id)
{
    const battery::ResolvedPackLayout resolved = resolvedLayout(layout);
    if (battery::BatteryPackEntity* pack = document.findPack(entity_id)) {
        pack->size = {resolved.pack_outer_width, resolved.stack.bounds_height, resolved.pack_outer_depth};
        pack->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = document.findCellGroup(entity_id)) {
        group->size = {resolved.group_width, resolved.stack.bounds_height, resolved.tray_depth};
        group->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        cell->radius = layout.cell_radius;
        cell->height = layout.cell_height;
        cell->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        const battery::ModuleBoundaryEntity* module = document.findModuleBoundary(busbar->parent_id);
        const battery::ModuleLayoutSlice slice = module != nullptr
            ? resolved.modules[static_cast<std::size_t>(std::clamp(module->module_index, 0, static_cast<int>(resolved.modules.size()) - 1))]
            : (resolved.modules.empty() ? battery::ModuleLayoutSlice{} : resolved.modules.front());
        busbar->size = {slice.busbar_length, layout.busbar_thickness, layout.busbar_width};
        busbar->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        const battery::ModuleBoundaryEntity* module = document.findModuleBoundary(plate->parent_id);
        const battery::ModuleLayoutSlice slice = module != nullptr
            ? resolved.modules[static_cast<std::size_t>(std::clamp(module->module_index, 0, static_cast<int>(resolved.modules.size()) - 1))]
            : (resolved.modules.empty() ? battery::ModuleLayoutSlice{} : resolved.modules.front());
        plate->size = {slice.cooling_width, layout.cooling_channel_thickness, resolved.cooling_depth};
        plate->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        const battery::ModuleLayoutSlice slice = resolved.modules[static_cast<std::size_t>(std::clamp(boundary->module_index, 0, static_cast<int>(resolved.modules.size()) - 1))];
        boundary->size = {slice.boundary_width, resolved.stack.bounds_height, resolved.boundary_depth};
        boundary->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        enclosure->size = {resolved.pack_outer_width, resolved.stack.bounds_height, resolved.pack_outer_depth};
        enclosure->wall_thickness = layout.enclosure_wall_thickness;
        enclosure->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadEditService::resetEntityLabelToGenerated(core::CadDocument& document, core::EntityId entity_id)
{
    if (battery::BatteryPackEntity* pack = document.findPack(entity_id)) {
        pack->label = "Battery pack";
        pack->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = document.findCellGroup(entity_id)) {
        group->label = "Cell group";
        group->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        cell->label = "Battery cell";
        cell->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        busbar->label = busbar->role == battery::BusbarRole::Negative ? "Negative busbar" : "Positive busbar";
        busbar->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        plate->label = "Cooling channel";
        plate->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        boundary->label = "Battery module";
        boundary->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        enclosure->label = "Enclosure";
        enclosure->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

} // namespace cad::edit
