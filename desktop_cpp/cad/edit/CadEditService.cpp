#include "CadEditService.h"

#include "../battery/PackLayoutGenerator.h"

namespace cad::edit {
namespace {

core::CadDocument buildGeneratedDocument(const battery::PackLayoutConfig& layout)
{
    core::CadDocument generated_document;
    battery::PackLayoutGenerator::rebuildDocument(generated_document, layout);
    return generated_document;
}

const battery::BatteryPackEntity* generatedPack(const core::CadDocument& document)
{
    return document.packs().empty() ? nullptr : &document.packs().front();
}

const battery::CellGroupEntity* generatedGroupBySeries(const core::CadDocument& document, int series_index)
{
    for (const auto& group : document.cellGroups()) {
        if (group.series_index == series_index) {
            return &group;
        }
    }
    return nullptr;
}

const battery::CellEntity* generatedCellByKey(
    const core::CadDocument& document,
    int series_index,
    int parallel_index)
{
    for (const auto& cell : document.cells()) {
        if (cell.series_index == series_index && cell.parallel_index == parallel_index) {
            return &cell;
        }
    }
    return nullptr;
}

const battery::BusbarEntity* generatedBusbarByKey(
    const core::CadDocument& document,
    int module_index,
    battery::BusbarRole role)
{
    for (const auto& busbar : document.busbars()) {
        if (busbar.module_index == module_index && busbar.role == role) {
            return &busbar;
        }
    }
    return nullptr;
}

const battery::CoolingPlateEntity* generatedCoolingPlateByIndex(const core::CadDocument& document, int plate_index)
{
    for (const auto& plate : document.coolingPlates()) {
        if (plate.plate_index == plate_index) {
            return &plate;
        }
    }
    return nullptr;
}

const battery::ModuleBoundaryEntity* generatedModuleByIndex(const core::CadDocument& document, int module_index)
{
    for (const auto& module : document.moduleBoundaries()) {
        if (module.module_index == module_index) {
            return &module;
        }
    }
    return nullptr;
}

const battery::PackEnclosureEntity* generatedEnclosureByIndex(const core::CadDocument& document, int enclosure_index)
{
    for (const auto& enclosure : document.packEnclosures()) {
        if (enclosure.enclosure_index == enclosure_index) {
            return &enclosure;
        }
    }
    return nullptr;
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
    const core::CadDocument generated = buildGeneratedDocument(layout);
    if (battery::BatteryPackEntity* pack = document.findPack(entity_id)) {
        const auto* generated_pack = generatedPack(generated);
        if (generated_pack == nullptr) {
            return false;
        }
        pack->center = generated_pack->center;
        pack->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = document.findCellGroup(entity_id)) {
        const auto* generated_group = generatedGroupBySeries(generated, group->series_index);
        if (generated_group == nullptr) {
            return false;
        }
        group->center = generated_group->center;
        group->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        const auto* generated_cell = generatedCellByKey(generated, cell->series_index, cell->parallel_index);
        if (generated_cell == nullptr) {
            return false;
        }
        cell->position = generated_cell->position;
        cell->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        const auto* generated_busbar = generatedBusbarByKey(generated, busbar->module_index, busbar->role);
        if (generated_busbar == nullptr) {
            return false;
        }
        busbar->center = generated_busbar->center;
        busbar->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        const auto* generated_plate = generatedCoolingPlateByIndex(generated, plate->plate_index);
        if (generated_plate == nullptr) {
            return false;
        }
        plate->center = generated_plate->center;
        plate->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        const auto* generated_boundary = generatedModuleByIndex(generated, boundary->module_index);
        if (generated_boundary == nullptr) {
            return false;
        }
        boundary->center = generated_boundary->center;
        boundary->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        const auto* generated_enclosure = generatedEnclosureByIndex(generated, enclosure->enclosure_index);
        if (generated_enclosure == nullptr) {
            return false;
        }
        enclosure->center = generated_enclosure->center;
        enclosure->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadEditService::resetEntityGeometryToGenerated(core::CadDocument& document, const battery::PackLayoutConfig& layout, core::EntityId entity_id)
{
    const core::CadDocument generated = buildGeneratedDocument(layout);
    if (battery::BatteryPackEntity* pack = document.findPack(entity_id)) {
        const auto* generated_pack = generatedPack(generated);
        if (generated_pack == nullptr) {
            return false;
        }
        pack->size = generated_pack->size;
        pack->series_count = generated_pack->series_count;
        pack->parallel_count = generated_pack->parallel_count;
        pack->cell_radius = generated_pack->cell_radius;
        pack->cell_height = generated_pack->cell_height;
        pack->spacing_x = generated_pack->spacing_x;
        pack->spacing_z = generated_pack->spacing_z;
        pack->layout_type = generated_pack->layout_type;
        pack->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = document.findCellGroup(entity_id)) {
        const auto* generated_group = generatedGroupBySeries(generated, group->series_index);
        if (generated_group == nullptr) {
            return false;
        }
        group->size = generated_group->size;
        group->cell_count = generated_group->cell_count;
        group->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        const auto* generated_cell = generatedCellByKey(generated, cell->series_index, cell->parallel_index);
        if (generated_cell == nullptr) {
            return false;
        }
        cell->form_factor = generated_cell->form_factor;
        cell->radius = generated_cell->radius;
        cell->height = generated_cell->height;
        cell->width = generated_cell->width;
        cell->depth = generated_cell->depth;
        cell->cell_type = generated_cell->cell_type;
        cell->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        const auto* generated_busbar = generatedBusbarByKey(generated, busbar->module_index, busbar->role);
        if (generated_busbar == nullptr) {
            return false;
        }
        busbar->size = generated_busbar->size;
        busbar->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        const auto* generated_plate = generatedCoolingPlateByIndex(generated, plate->plate_index);
        if (generated_plate == nullptr) {
            return false;
        }
        plate->size = generated_plate->size;
        plate->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        const auto* generated_boundary = generatedModuleByIndex(generated, boundary->module_index);
        if (generated_boundary == nullptr) {
            return false;
        }
        boundary->size = generated_boundary->size;
        boundary->series_span = generated_boundary->series_span;
        boundary->parallel_span = generated_boundary->parallel_span;
        boundary->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        const auto* generated_enclosure = generatedEnclosureByIndex(generated, enclosure->enclosure_index);
        if (generated_enclosure == nullptr) {
            return false;
        }
        enclosure->size = generated_enclosure->size;
        enclosure->wall_thickness = generated_enclosure->wall_thickness;
        enclosure->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadEditService::resetEntityLabelToGenerated(core::CadDocument& document, const battery::PackLayoutConfig& layout, core::EntityId entity_id)
{
    const core::CadDocument generated = buildGeneratedDocument(layout);
    if (battery::BatteryPackEntity* pack = document.findPack(entity_id)) {
        const auto* generated_pack = generatedPack(generated);
        if (generated_pack == nullptr) {
            return false;
        }
        pack->label = generated_pack->label;
        pack->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = document.findCellGroup(entity_id)) {
        const auto* generated_group = generatedGroupBySeries(generated, group->series_index);
        if (generated_group == nullptr) {
            return false;
        }
        group->label = generated_group->label;
        group->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = document.findCell(entity_id)) {
        const auto* generated_cell = generatedCellByKey(generated, cell->series_index, cell->parallel_index);
        if (generated_cell == nullptr) {
            return false;
        }
        cell->label = generated_cell->label;
        cell->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = document.findBusbar(entity_id)) {
        const auto* generated_busbar = generatedBusbarByKey(generated, busbar->module_index, busbar->role);
        if (generated_busbar == nullptr) {
            return false;
        }
        busbar->label = generated_busbar->label;
        busbar->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = document.findCoolingPlate(entity_id)) {
        const auto* generated_plate = generatedCoolingPlateByIndex(generated, plate->plate_index);
        if (generated_plate == nullptr) {
            return false;
        }
        plate->label = generated_plate->label;
        plate->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = document.findModuleBoundary(entity_id)) {
        const auto* generated_boundary = generatedModuleByIndex(generated, boundary->module_index);
        if (generated_boundary == nullptr) {
            return false;
        }
        boundary->label = generated_boundary->label;
        boundary->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = document.findEnclosure(entity_id)) {
        const auto* generated_enclosure = generatedEnclosureByIndex(generated, enclosure->enclosure_index);
        if (generated_enclosure == nullptr) {
            return false;
        }
        enclosure->label = generated_enclosure->label;
        enclosure->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

} // namespace cad::edit
