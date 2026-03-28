#include "CadDocument.h"

#include <type_traits>
#include <utility>

namespace cad::core {
namespace {

template <typename T>
battery::EntitySummary summaryFromEntity(const T& entity)
{
    return {entity.id, entity.kind, entity.label, entity.visible, entity.parent_id, entity.simulation_group_index};
}

template <typename T>
void assignBaseProperties(T& entity, const battery::EntitySummary& summary, const battery::EntityPropertyModes& property_modes)
{
    entity.label = summary.label;
    entity.visible = summary.visible;
    entity.property_modes = property_modes;
}

template <typename T>
bool setVisibility(T* entity, bool visible)
{
    if (entity == nullptr) {
        return false;
    }
    entity->visible = visible;
    entity->property_modes.visibility = battery::PropertyMode::UserOverride;
    return true;
}

} // namespace

void CadDocument::clear()
{
    m_selection.clear();
    m_entityIndex.clear();
    m_packs.clear();
    m_cellGroups.clear();
    m_cells.clear();
    m_busbars.clear();
    m_coolingPlates.clear();
    m_moduleBoundaries.clear();
    m_packEnclosures.clear();
}

EntityId CadDocument::allocateEntityId()
{
    return EntityId{m_nextEntityId++};
}

battery::BatteryPackEntity& CadDocument::addPack(battery::BatteryPackEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::BatteryPack;
    m_packs.push_back(std::move(entity));
    indexEntity(m_packs.back().id, battery::EntityKind::BatteryPack, m_packs.size() - 1);
    return m_packs.back();
}

battery::CellGroupEntity& CadDocument::addCellGroup(battery::CellGroupEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::CellGroup;
    m_cellGroups.push_back(std::move(entity));
    indexEntity(m_cellGroups.back().id, battery::EntityKind::CellGroup, m_cellGroups.size() - 1);
    return m_cellGroups.back();
}

battery::CellEntity& CadDocument::addCell(battery::CellEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::Cell;
    m_cells.push_back(std::move(entity));
    indexEntity(m_cells.back().id, battery::EntityKind::Cell, m_cells.size() - 1);
    return m_cells.back();
}

battery::BusbarEntity& CadDocument::addBusbar(battery::BusbarEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::Busbar;
    m_busbars.push_back(std::move(entity));
    indexEntity(m_busbars.back().id, battery::EntityKind::Busbar, m_busbars.size() - 1);
    return m_busbars.back();
}

battery::CoolingPlateEntity& CadDocument::addCoolingPlate(battery::CoolingPlateEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::CoolingPlate;
    m_coolingPlates.push_back(std::move(entity));
    indexEntity(m_coolingPlates.back().id, battery::EntityKind::CoolingPlate, m_coolingPlates.size() - 1);
    return m_coolingPlates.back();
}

battery::ModuleBoundaryEntity& CadDocument::addModuleBoundary(battery::ModuleBoundaryEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::ModuleBoundary;
    m_moduleBoundaries.push_back(std::move(entity));
    indexEntity(m_moduleBoundaries.back().id, battery::EntityKind::ModuleBoundary, m_moduleBoundaries.size() - 1);
    return m_moduleBoundaries.back();
}

battery::PackEnclosureEntity& CadDocument::addPackEnclosure(battery::PackEnclosureEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::PackEnclosure;
    m_packEnclosures.push_back(std::move(entity));
    indexEntity(m_packEnclosures.back().id, battery::EntityKind::PackEnclosure, m_packEnclosures.size() - 1);
    return m_packEnclosures.back();
}

bool CadDocument::removeEntity(EntityId id)
{
    for (EntityId child_id : childIds(id)) {
        removeEntity(child_id);
    }

    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr) {
        return false;
    }

    switch (locator->kind) {
    case battery::EntityKind::BatteryPack:
        m_packs.erase(m_packs.begin() + static_cast<std::ptrdiff_t>(locator->index));
        break;
    case battery::EntityKind::CellGroup:
        m_cellGroups.erase(m_cellGroups.begin() + static_cast<std::ptrdiff_t>(locator->index));
        break;
    case battery::EntityKind::Cell:
        m_cells.erase(m_cells.begin() + static_cast<std::ptrdiff_t>(locator->index));
        break;
    case battery::EntityKind::Busbar:
        m_busbars.erase(m_busbars.begin() + static_cast<std::ptrdiff_t>(locator->index));
        break;
    case battery::EntityKind::CoolingPlate:
        m_coolingPlates.erase(m_coolingPlates.begin() + static_cast<std::ptrdiff_t>(locator->index));
        break;
    case battery::EntityKind::ModuleBoundary:
        m_moduleBoundaries.erase(m_moduleBoundaries.begin() + static_cast<std::ptrdiff_t>(locator->index));
        break;
    case battery::EntityKind::PackEnclosure:
        m_packEnclosures.erase(m_packEnclosures.begin() + static_cast<std::ptrdiff_t>(locator->index));
        break;
    }

    if (m_selection.primary == id) {
        m_selection.clear();
    }
    rebuildIndex();
    return true;
}

bool CadDocument::moveEntity(EntityId id, const math::Vec3& delta)
{
    if (battery::BatteryPackEntity* pack = findPack(id)) {
        pack->center = math::add(pack->center, delta);
        pack->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CellGroupEntity* group = findCellGroup(id)) {
        group->center = math::add(group->center, delta);
        group->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CellEntity* cell = findCell(id)) {
        cell->position = math::add(cell->position, delta);
        cell->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->center = math::add(busbar->center, delta);
        busbar->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->center = math::add(plate->center, delta);
        plate->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->center = math::add(boundary->center, delta);
        boundary->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->center = math::add(enclosure->center, delta);
        enclosure->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::setEntityPosition(EntityId id, const math::Vec3& position)
{
    if (battery::BatteryPackEntity* pack = findPack(id)) {
        pack->center = position;
        pack->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CellGroupEntity* group = findCellGroup(id)) {
        group->center = position;
        group->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CellEntity* cell = findCell(id)) {
        cell->position = position;
        cell->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->center = position;
        busbar->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->center = position;
        plate->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->center = position;
        boundary->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->center = position;
        enclosure->property_modes.position = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::setEntityLabel(EntityId id, std::string label)
{
    if (battery::BatteryPackEntity* pack = findPack(id)) {
        pack->label = std::move(label);
        pack->property_modes.label = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CellGroupEntity* group = findCellGroup(id)) {
        group->label = std::move(label);
        group->property_modes.label = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CellEntity* cell = findCell(id)) {
        cell->label = std::move(label);
        cell->property_modes.label = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->label = std::move(label);
        busbar->property_modes.label = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->label = std::move(label);
        plate->property_modes.label = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->label = std::move(label);
        boundary->property_modes.label = battery::PropertyMode::UserOverride;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->label = std::move(label);
        enclosure->property_modes.label = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::setEntityVisibility(EntityId id, bool visible)
{
    if (setVisibility(findPack(id), visible)) {
        return true;
    }
    if (setVisibility(findCellGroup(id), visible)) {
        return true;
    }
    if (setVisibility(findCell(id), visible)) {
        return true;
    }
    if (setVisibility(findBusbar(id), visible)) {
        return true;
    }
    if (setVisibility(findCoolingPlate(id), visible)) {
        return true;
    }
    if (setVisibility(findModuleBoundary(id), visible)) {
        return true;
    }
    return setVisibility(findEnclosure(id), visible);
}

void CadDocument::selectEntity(EntityId id)
{
    if (hasEntity(id)) {
        m_selection.primary = id;
    }
}

void CadDocument::clearSelection()
{
    m_selection.clear();
}

bool CadDocument::setCellPosition(EntityId id, const math::Vec3& position)
{
    return setEntityPosition(id, position);
}

bool CadDocument::setCellGeometry(EntityId id, float radius, float height)
{
    if (battery::CellEntity* cell = findCell(id)) {
        cell->radius = radius;
        cell->height = height;
        cell->property_modes.geometry = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::setBusbarGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size)
{
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->center = center;
        busbar->size = size;
        busbar->property_modes.position = battery::PropertyMode::UserOverride;
        busbar->property_modes.geometry = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::setCoolingPlateGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size)
{
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->center = center;
        plate->size = size;
        plate->property_modes.position = battery::PropertyMode::UserOverride;
        plate->property_modes.geometry = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::setModuleBoundaryGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size)
{
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->center = center;
        boundary->size = size;
        boundary->property_modes.position = battery::PropertyMode::UserOverride;
        boundary->property_modes.geometry = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::setEnclosureGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size, float wall_thickness)
{
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->center = center;
        enclosure->size = size;
        enclosure->wall_thickness = wall_thickness;
        enclosure->property_modes.position = battery::PropertyMode::UserOverride;
        enclosure->property_modes.geometry = battery::PropertyMode::UserOverride;
        return true;
    }
    return false;
}

bool CadDocument::updateCellProperties(EntityId id, const battery::CellPropertiesUpdate& update)
{
    battery::CellEntity* cell = findCell(id);
    if (cell == nullptr) {
        return false;
    }
    if (update.position.has_value()) {
        cell->position = *update.position;
        cell->property_modes.position = battery::PropertyMode::UserOverride;
    }
    if (update.radius.has_value()) {
        cell->radius = *update.radius;
        cell->property_modes.geometry = battery::PropertyMode::UserOverride;
    }
    if (update.height.has_value()) {
        cell->height = *update.height;
        cell->property_modes.geometry = battery::PropertyMode::UserOverride;
    }
    if (update.label.has_value()) {
        cell->label = *update.label;
        cell->property_modes.label = battery::PropertyMode::UserOverride;
    }
    if (update.visible.has_value()) {
        cell->visible = *update.visible;
        cell->property_modes.visibility = battery::PropertyMode::UserOverride;
    }
    return true;
}

bool CadDocument::updateBusbarProperties(EntityId id, const battery::BusbarPropertiesUpdate& update)
{
    battery::BusbarEntity* busbar = findBusbar(id);
    if (busbar == nullptr) {
        return false;
    }
    if (update.center.has_value()) {
        busbar->center = *update.center;
        busbar->property_modes.position = battery::PropertyMode::UserOverride;
    }
    if (update.size.has_value()) {
        busbar->size = *update.size;
        busbar->property_modes.geometry = battery::PropertyMode::UserOverride;
    }
    if (update.label.has_value()) {
        busbar->label = *update.label;
        busbar->property_modes.label = battery::PropertyMode::UserOverride;
    }
    if (update.visible.has_value()) {
        busbar->visible = *update.visible;
        busbar->property_modes.visibility = battery::PropertyMode::UserOverride;
    }
    return true;
}

bool CadDocument::updateCoolingPlateProperties(EntityId id, const battery::CoolingPlatePropertiesUpdate& update)
{
    battery::CoolingPlateEntity* plate = findCoolingPlate(id);
    if (plate == nullptr) {
        return false;
    }
    if (update.center.has_value()) {
        plate->center = *update.center;
        plate->property_modes.position = battery::PropertyMode::UserOverride;
    }
    if (update.size.has_value()) {
        plate->size = *update.size;
        plate->property_modes.geometry = battery::PropertyMode::UserOverride;
    }
    if (update.label.has_value()) {
        plate->label = *update.label;
        plate->property_modes.label = battery::PropertyMode::UserOverride;
    }
    if (update.visible.has_value()) {
        plate->visible = *update.visible;
        plate->property_modes.visibility = battery::PropertyMode::UserOverride;
    }
    return true;
}

bool CadDocument::updateModuleBoundaryProperties(EntityId id, const battery::ModuleBoundaryPropertiesUpdate& update)
{
    battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id);
    if (boundary == nullptr) {
        return false;
    }
    if (update.center.has_value()) {
        boundary->center = *update.center;
        boundary->property_modes.position = battery::PropertyMode::UserOverride;
    }
    if (update.size.has_value()) {
        boundary->size = *update.size;
        boundary->property_modes.geometry = battery::PropertyMode::UserOverride;
    }
    if (update.label.has_value()) {
        boundary->label = *update.label;
        boundary->property_modes.label = battery::PropertyMode::UserOverride;
    }
    if (update.visible.has_value()) {
        boundary->visible = *update.visible;
        boundary->property_modes.visibility = battery::PropertyMode::UserOverride;
    }
    return true;
}

bool CadDocument::updateEnclosureProperties(EntityId id, const battery::PackEnclosurePropertiesUpdate& update)
{
    battery::PackEnclosureEntity* enclosure = findEnclosure(id);
    if (enclosure == nullptr) {
        return false;
    }
    if (update.center.has_value()) {
        enclosure->center = *update.center;
        enclosure->property_modes.position = battery::PropertyMode::UserOverride;
    }
    if (update.size.has_value()) {
        enclosure->size = *update.size;
        enclosure->property_modes.geometry = battery::PropertyMode::UserOverride;
    }
    if (update.wall_thickness.has_value()) {
        enclosure->wall_thickness = *update.wall_thickness;
        enclosure->property_modes.geometry = battery::PropertyMode::UserOverride;
    }
    if (update.label.has_value()) {
        enclosure->label = *update.label;
        enclosure->property_modes.label = battery::PropertyMode::UserOverride;
    }
    if (update.visible.has_value()) {
        enclosure->visible = *update.visible;
        enclosure->property_modes.visibility = battery::PropertyMode::UserOverride;
    }
    return true;
}

bool CadDocument::applyCellProperties(EntityId id, const battery::CellProperties& properties)
{
    battery::CellEntity* cell = findCell(id);
    if (cell == nullptr) {
        return false;
    }
    assignBaseProperties(*cell, properties.summary, properties.property_modes);
    cell->position = properties.position;
    cell->radius = properties.radius;
    cell->height = properties.height;
    return true;
}

bool CadDocument::applyBusbarProperties(EntityId id, const battery::BusbarProperties& properties)
{
    battery::BusbarEntity* busbar = findBusbar(id);
    if (busbar == nullptr) {
        return false;
    }
    assignBaseProperties(*busbar, properties.summary, properties.property_modes);
    busbar->center = properties.center;
    busbar->size = properties.size;
    busbar->role = properties.role;
    return true;
}

bool CadDocument::applyCoolingPlateProperties(EntityId id, const battery::CoolingPlateProperties& properties)
{
    battery::CoolingPlateEntity* plate = findCoolingPlate(id);
    if (plate == nullptr) {
        return false;
    }
    assignBaseProperties(*plate, properties.summary, properties.property_modes);
    plate->center = properties.center;
    plate->size = properties.size;
    plate->plate_index = properties.plate_index;
    return true;
}

bool CadDocument::applyModuleBoundaryProperties(EntityId id, const battery::ModuleBoundaryProperties& properties)
{
    battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id);
    if (boundary == nullptr) {
        return false;
    }
    assignBaseProperties(*boundary, properties.summary, properties.property_modes);
    boundary->center = properties.center;
    boundary->size = properties.size;
    boundary->module_index = properties.module_index;
    return true;
}

bool CadDocument::applyEnclosureProperties(EntityId id, const battery::PackEnclosureProperties& properties)
{
    battery::PackEnclosureEntity* enclosure = findEnclosure(id);
    if (enclosure == nullptr) {
        return false;
    }
    assignBaseProperties(*enclosure, properties.summary, properties.property_modes);
    enclosure->center = properties.center;
    enclosure->size = properties.size;
    enclosure->wall_thickness = properties.wall_thickness;
    enclosure->enclosure_index = properties.enclosure_index;
    return true;
}

bool CadDocument::resetEntityPositionToGenerated(EntityId id)
{
    if (battery::BatteryPackEntity* pack = findPack(id)) {
        pack->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = findCellGroup(id)) {
        group->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = findCell(id)) {
        cell->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->property_modes.position = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadDocument::resetEntityGeometryToGenerated(EntityId id)
{
    if (battery::BatteryPackEntity* pack = findPack(id)) {
        pack->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = findCellGroup(id)) {
        group->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = findCell(id)) {
        cell->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->property_modes.geometry = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadDocument::resetEntityLabelToGenerated(EntityId id)
{
    if (battery::BatteryPackEntity* pack = findPack(id)) {
        pack->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellGroupEntity* group = findCellGroup(id)) {
        group->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CellEntity* cell = findCell(id)) {
        cell->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->property_modes.label = battery::PropertyMode::Generated;
        return true;
    }
    return false;
}

bool CadDocument::hasEntity(EntityId id) const
{
    return findLocator(id) != nullptr;
}

std::optional<battery::EntityKind> CadDocument::entityKind(EntityId id) const
{
    if (const EntityLocator* locator = findLocator(id)) {
        return locator->kind;
    }
    return std::nullopt;
}

battery::BatteryPackEntity* CadDocument::findPack(EntityId id)
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::BatteryPack) {
        return nullptr;
    }
    return &m_packs[locator->index];
}

const battery::BatteryPackEntity* CadDocument::findPack(EntityId id) const
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::BatteryPack) {
        return nullptr;
    }
    return &m_packs[locator->index];
}

battery::CellGroupEntity* CadDocument::findCellGroup(EntityId id)
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::CellGroup) {
        return nullptr;
    }
    return &m_cellGroups[locator->index];
}

const battery::CellGroupEntity* CadDocument::findCellGroup(EntityId id) const
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::CellGroup) {
        return nullptr;
    }
    return &m_cellGroups[locator->index];
}

battery::CellEntity* CadDocument::findCell(EntityId id)
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::Cell) {
        return nullptr;
    }
    return &m_cells[locator->index];
}

const battery::CellEntity* CadDocument::findCell(EntityId id) const
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::Cell) {
        return nullptr;
    }
    return &m_cells[locator->index];
}

battery::BusbarEntity* CadDocument::findBusbar(EntityId id)
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::Busbar) {
        return nullptr;
    }
    return &m_busbars[locator->index];
}

const battery::BusbarEntity* CadDocument::findBusbar(EntityId id) const
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::Busbar) {
        return nullptr;
    }
    return &m_busbars[locator->index];
}

battery::CoolingPlateEntity* CadDocument::findCoolingPlate(EntityId id)
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::CoolingPlate) {
        return nullptr;
    }
    return &m_coolingPlates[locator->index];
}

const battery::CoolingPlateEntity* CadDocument::findCoolingPlate(EntityId id) const
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::CoolingPlate) {
        return nullptr;
    }
    return &m_coolingPlates[locator->index];
}

battery::ModuleBoundaryEntity* CadDocument::findModuleBoundary(EntityId id)
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::ModuleBoundary) {
        return nullptr;
    }
    return &m_moduleBoundaries[locator->index];
}

const battery::ModuleBoundaryEntity* CadDocument::findModuleBoundary(EntityId id) const
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::ModuleBoundary) {
        return nullptr;
    }
    return &m_moduleBoundaries[locator->index];
}

battery::PackEnclosureEntity* CadDocument::findEnclosure(EntityId id)
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::PackEnclosure) {
        return nullptr;
    }
    return &m_packEnclosures[locator->index];
}

const battery::PackEnclosureEntity* CadDocument::findEnclosure(EntityId id) const
{
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr || locator->kind != battery::EntityKind::PackEnclosure) {
        return nullptr;
    }
    return &m_packEnclosures[locator->index];
}

std::optional<battery::EntityRecord> CadDocument::snapshotEntity(EntityId id) const
{
    if (const battery::BatteryPackEntity* pack = findPack(id)) {
        return *pack;
    }
    if (const battery::CellGroupEntity* group = findCellGroup(id)) {
        return *group;
    }
    if (const battery::CellEntity* cell = findCell(id)) {
        return *cell;
    }
    if (const battery::BusbarEntity* busbar = findBusbar(id)) {
        return *busbar;
    }
    if (const battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        return *plate;
    }
    if (const battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        return *boundary;
    }
    if (const battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        return *enclosure;
    }
    return std::nullopt;
}

std::optional<battery::EntitySummary> CadDocument::getEntitySummary(EntityId id) const
{
    if (const battery::BatteryPackEntity* pack = findPack(id)) {
        return summaryFromEntity(*pack);
    }
    if (const battery::CellGroupEntity* group = findCellGroup(id)) {
        return summaryFromEntity(*group);
    }
    if (const battery::CellEntity* cell = findCell(id)) {
        return summaryFromEntity(*cell);
    }
    if (const battery::BusbarEntity* busbar = findBusbar(id)) {
        return summaryFromEntity(*busbar);
    }
    if (const battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        return summaryFromEntity(*plate);
    }
    if (const battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        return summaryFromEntity(*boundary);
    }
    if (const battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        return summaryFromEntity(*enclosure);
    }
    return std::nullopt;
}

std::optional<battery::EntitySummary> CadDocument::getSelectedEntitySummary() const
{
    return getEntitySummary(m_selection.primary);
}

std::optional<battery::CellProperties> CadDocument::getCellProperties(EntityId id) const
{
    const battery::CellEntity* cell = findCell(id);
    if (cell == nullptr) {
        return std::nullopt;
    }
    return battery::CellProperties{
        summaryFromEntity(*cell),
        cell->position,
        cell->radius,
        cell->height,
        cell->series_index,
        cell->parallel_index,
        cell->property_modes
    };
}

std::optional<battery::BusbarProperties> CadDocument::getBusbarProperties(EntityId id) const
{
    const battery::BusbarEntity* busbar = findBusbar(id);
    if (busbar == nullptr) {
        return std::nullopt;
    }
    return battery::BusbarProperties{
        summaryFromEntity(*busbar),
        busbar->role,
        busbar->center,
        busbar->size,
        busbar->property_modes
    };
}

std::optional<battery::CoolingPlateProperties> CadDocument::getCoolingPlateProperties(EntityId id) const
{
    const battery::CoolingPlateEntity* plate = findCoolingPlate(id);
    if (plate == nullptr) {
        return std::nullopt;
    }
    return battery::CoolingPlateProperties{
        summaryFromEntity(*plate),
        plate->plate_index,
        plate->center,
        plate->size,
        plate->property_modes
    };
}

std::optional<battery::ModuleBoundaryProperties> CadDocument::getModuleBoundaryProperties(EntityId id) const
{
    const battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id);
    if (boundary == nullptr) {
        return std::nullopt;
    }
    return battery::ModuleBoundaryProperties{
        summaryFromEntity(*boundary),
        boundary->module_index,
        boundary->center,
        boundary->size,
        boundary->property_modes
    };
}

std::optional<battery::PackEnclosureProperties> CadDocument::getEnclosureProperties(EntityId id) const
{
    const battery::PackEnclosureEntity* enclosure = findEnclosure(id);
    if (enclosure == nullptr) {
        return std::nullopt;
    }
    return battery::PackEnclosureProperties{
        summaryFromEntity(*enclosure),
        enclosure->enclosure_index,
        enclosure->center,
        enclosure->size,
        enclosure->wall_thickness,
        enclosure->property_modes
    };
}

std::vector<EntityId> CadDocument::childIds(EntityId parent_id) const
{
    std::vector<EntityId> children;
    auto collect = [&children, parent_id](const auto& entities) {
        for (const auto& entity : entities) {
            if (entity.parent_id == parent_id) {
                children.push_back(entity.id);
            }
        }
    };
    collect(m_packs);
    collect(m_moduleBoundaries);
    collect(m_cellGroups);
    collect(m_cells);
    collect(m_busbars);
    collect(m_coolingPlates);
    collect(m_packEnclosures);
    return children;
}

std::vector<EntityId> CadDocument::subtreeIds(EntityId root_id) const
{
    std::vector<EntityId> ids;
    if (!hasEntity(root_id)) {
        return ids;
    }
    ids.push_back(root_id);
    for (EntityId child_id : childIds(root_id)) {
        const std::vector<EntityId> child_subtree = subtreeIds(child_id);
        ids.insert(ids.end(), child_subtree.begin(), child_subtree.end());
    }
    return ids;
}

math::Vec3 CadDocument::worldPosition(EntityId id) const
{
    auto accumulate = [this](const auto* entity, const math::Vec3& local) -> math::Vec3 {
        if (entity == nullptr) {
            return {};
        }
        if (!entity->parent_id.isValid()) {
            return local;
        }
        return math::add(local, worldPosition(entity->parent_id));
    };

    if (const auto* pack = findPack(id)) {
        return accumulate(pack, pack->center);
    }
    if (const auto* module = findModuleBoundary(id)) {
        return accumulate(module, module->center);
    }
    if (const auto* group = findCellGroup(id)) {
        return accumulate(group, group->center);
    }
    if (const auto* cell = findCell(id)) {
        return accumulate(cell, cell->position);
    }
    if (const auto* busbar = findBusbar(id)) {
        return accumulate(busbar, busbar->center);
    }
    if (const auto* plate = findCoolingPlate(id)) {
        return accumulate(plate, plate->center);
    }
    if (const auto* enclosure = findEnclosure(id)) {
        return accumulate(enclosure, enclosure->center);
    }
    return {};
}

battery::BoundingBox CadDocument::worldBounds(EntityId id) const
{
    if (const auto* pack = findPack(id)) {
        return {worldPosition(id), pack->size};
    }
    if (const auto* module = findModuleBoundary(id)) {
        return {worldPosition(id), module->size};
    }
    if (const auto* group = findCellGroup(id)) {
        return {worldPosition(id), group->size};
    }
    if (const auto* cell = findCell(id)) {
        return {worldPosition(id), {cell->radius * 2.0f, cell->height, cell->radius * 2.0f}};
    }
    if (const auto* busbar = findBusbar(id)) {
        return {worldPosition(id), busbar->size};
    }
    if (const auto* plate = findCoolingPlate(id)) {
        return {worldPosition(id), plate->size};
    }
    if (const auto* enclosure = findEnclosure(id)) {
        return {worldPosition(id), enclosure->size};
    }
    return {};
}

void CadDocument::rebuildIndex()
{
    m_entityIndex.clear();
    for (std::size_t i = 0; i < m_packs.size(); ++i) {
        indexEntity(m_packs[i].id, battery::EntityKind::BatteryPack, i);
    }
    for (std::size_t i = 0; i < m_cellGroups.size(); ++i) {
        indexEntity(m_cellGroups[i].id, battery::EntityKind::CellGroup, i);
    }
    for (std::size_t i = 0; i < m_cells.size(); ++i) {
        indexEntity(m_cells[i].id, battery::EntityKind::Cell, i);
    }
    for (std::size_t i = 0; i < m_busbars.size(); ++i) {
        indexEntity(m_busbars[i].id, battery::EntityKind::Busbar, i);
    }
    for (std::size_t i = 0; i < m_coolingPlates.size(); ++i) {
        indexEntity(m_coolingPlates[i].id, battery::EntityKind::CoolingPlate, i);
    }
    for (std::size_t i = 0; i < m_moduleBoundaries.size(); ++i) {
        indexEntity(m_moduleBoundaries[i].id, battery::EntityKind::ModuleBoundary, i);
    }
    for (std::size_t i = 0; i < m_packEnclosures.size(); ++i) {
        indexEntity(m_packEnclosures[i].id, battery::EntityKind::PackEnclosure, i);
    }
}

void CadDocument::indexEntity(EntityId id, battery::EntityKind kind, std::size_t index)
{
    m_entityIndex[id] = EntityLocator{kind, index};
}

CadDocument::EntityLocator* CadDocument::findLocator(EntityId id)
{
    auto it = m_entityIndex.find(id);
    return it == m_entityIndex.end() ? nullptr : &it->second;
}

const CadDocument::EntityLocator* CadDocument::findLocator(EntityId id) const
{
    auto it = m_entityIndex.find(id);
    return it == m_entityIndex.end() ? nullptr : &it->second;
}

bool CadDocument::restoreEntity(const battery::EntityRecord& entity)
{
    return std::visit([this](const auto& value) -> bool {
        using T = std::decay_t<decltype(value)>;
        if (hasEntity(value.id)) {
            if constexpr (std::is_same_v<T, battery::BatteryPackEntity>) {
                m_packs[findLocator(value.id)->index] = value;
            } else if constexpr (std::is_same_v<T, battery::CellGroupEntity>) {
                m_cellGroups[findLocator(value.id)->index] = value;
            } else if constexpr (std::is_same_v<T, battery::CellEntity>) {
                m_cells[findLocator(value.id)->index] = value;
            } else if constexpr (std::is_same_v<T, battery::BusbarEntity>) {
                m_busbars[findLocator(value.id)->index] = value;
            } else if constexpr (std::is_same_v<T, battery::CoolingPlateEntity>) {
                m_coolingPlates[findLocator(value.id)->index] = value;
            } else if constexpr (std::is_same_v<T, battery::ModuleBoundaryEntity>) {
                m_moduleBoundaries[findLocator(value.id)->index] = value;
            } else if constexpr (std::is_same_v<T, battery::PackEnclosureEntity>) {
                m_packEnclosures[findLocator(value.id)->index] = value;
            }
            rebuildIndex();
            return true;
        }

        if constexpr (std::is_same_v<T, battery::BatteryPackEntity>) {
            addPack(value);
        } else if constexpr (std::is_same_v<T, battery::CellGroupEntity>) {
            addCellGroup(value);
        } else if constexpr (std::is_same_v<T, battery::CellEntity>) {
            addCell(value);
        } else if constexpr (std::is_same_v<T, battery::BusbarEntity>) {
            addBusbar(value);
        } else if constexpr (std::is_same_v<T, battery::CoolingPlateEntity>) {
            addCoolingPlate(value);
        } else if constexpr (std::is_same_v<T, battery::ModuleBoundaryEntity>) {
            addModuleBoundary(value);
        } else if constexpr (std::is_same_v<T, battery::PackEnclosureEntity>) {
            addPackEnclosure(value);
        }
        return true;
    }, entity);
}

} // namespace cad::core
