#include "CadEditService.h"

#include <algorithm>

namespace cad::edit {
namespace {

float packWidth(const battery::PackLayoutConfig& config)
{
    const int series_count = std::max(1, config.cells_in_series);
    return std::max(240.0f, (series_count - 1) * config.x_spacing + config.cell_radius * 2.8f);
}

float packDepth(const battery::PackLayoutConfig& config)
{
    const int parallel_count = std::max(1, config.cells_in_parallel);
    return std::max(180.0f, (parallel_count - 1) * config.z_spacing + config.cell_radius * 2.8f);
}

math::Vec3 generatedCellPosition(const battery::PackLayoutConfig& config, int series_index, int parallel_index)
{
    const int series_count = std::max(1, config.cells_in_series);
    const int parallel_count = std::max(1, config.cells_in_parallel);
    return {
        (series_index - (series_count - 1) / 2.0f) * config.x_spacing,
        0.0f,
        (parallel_index - (parallel_count - 1) / 2.0f) * config.z_spacing
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
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        cell->position = generatedCellPosition(layout, cell->series_index, cell->parallel_index);
        cell->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        const float depth = packDepth(layout);
        busbar->center = {0.0f, 112.0f, busbar->role == battery::BusbarRole::Negative ? -depth * 0.5f : depth * 0.5f};
        busbar->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        plate->center = {0.0f, -128.0f, 0.0f};
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
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        cell->radius = layout.cell_radius;
        cell->height = layout.cell_height;
        cell->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        busbar->size = {packWidth(layout) + 96.0f, 12.0f, 16.0f};
        busbar->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        plate->size = {packWidth(layout) + 72.0f, 24.0f, packDepth(layout) + 92.0f};
        plate->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        boundary->size = {packWidth(layout) + 96.0f, layout.cell_height + 56.0f, packDepth(layout) + 92.0f};
        boundary->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        enclosure->size = {packWidth(layout) + 140.0f, layout.cell_height + 92.0f, packDepth(layout) + 136.0f};
        enclosure->wall_thickness = 8.0f;
        enclosure->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadEditService::resetEntityLabelToGenerated(core::CadDocument& document, core::EntityId entity_id)
{
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        cell->label = "Cell";
        cell->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        busbar->label = busbar->role == battery::BusbarRole::Negative ? "Negative busbar" : "Positive busbar";
        busbar->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        plate->label = "Cooling plate";
        plate->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        boundary->label = "Module boundary";
        boundary->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        enclosure->label = "Pack enclosure";
        enclosure->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

} // namespace cad::edit
