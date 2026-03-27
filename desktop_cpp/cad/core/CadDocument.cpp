#include "CadDocument.h"

#include <type_traits>
#include <utility>

namespace cad::core {
namespace {

template <typename T>
battery::EntitySummary summaryFromEntity(const T& entity)
{
    return {entity.id, entity.kind, entity.label, entity.visible};
}

} // namespace

void CadDocument::clear()
{
    m_selection.clear();
    m_entityIndex.clear();
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
    const EntityLocator* locator = findLocator(id);
    if (locator == nullptr) {
        return false;
    }

    switch (locator->kind) {
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
    if (battery::CellEntity* cell = findCell(id)) {
        cell->position = math::add(cell->position, delta);
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->center = math::add(busbar->center, delta);
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->center = math::add(plate->center, delta);
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->center = math::add(boundary->center, delta);
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->center = math::add(enclosure->center, delta);
        return true;
    }
    return false;
}

bool CadDocument::setEntityPosition(EntityId id, const math::Vec3& position)
{
    if (battery::CellEntity* cell = findCell(id)) {
        cell->position = position;
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->center = position;
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->center = position;
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->center = position;
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->center = position;
        return true;
    }
    return false;
}

bool CadDocument::setEntityLabel(EntityId id, std::string label)
{
    if (battery::CellEntity* cell = findCell(id)) {
        cell->label = std::move(label);
        return true;
    }
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->label = std::move(label);
        return true;
    }
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->label = std::move(label);
        return true;
    }
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->label = std::move(label);
        return true;
    }
    if (battery::PackEnclosureEntity* enclosure = findEnclosure(id)) {
        enclosure->label = std::move(label);
        return true;
    }
    return false;
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
    if (battery::CellEntity* cell = findCell(id)) {
        cell->position = position;
        return true;
    }
    return false;
}

bool CadDocument::setCellGeometry(EntityId id, float radius, float height)
{
    if (battery::CellEntity* cell = findCell(id)) {
        cell->radius = radius;
        cell->height = height;
        return true;
    }
    return false;
}

bool CadDocument::setBusbarGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size)
{
    if (battery::BusbarEntity* busbar = findBusbar(id)) {
        busbar->center = center;
        busbar->size = size;
        return true;
    }
    return false;
}

bool CadDocument::setCoolingPlateGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size)
{
    if (battery::CoolingPlateEntity* plate = findCoolingPlate(id)) {
        plate->center = center;
        plate->size = size;
        return true;
    }
    return false;
}

bool CadDocument::setModuleBoundaryGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size)
{
    if (battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id)) {
        boundary->center = center;
        boundary->size = size;
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
    return battery::CellProperties{summaryFromEntity(*cell), cell->position, cell->radius, cell->height, cell->series_index, cell->parallel_index};
}

std::optional<battery::BusbarProperties> CadDocument::getBusbarProperties(EntityId id) const
{
    const battery::BusbarEntity* busbar = findBusbar(id);
    if (busbar == nullptr) {
        return std::nullopt;
    }
    return battery::BusbarProperties{summaryFromEntity(*busbar), busbar->role, busbar->center, busbar->size};
}

std::optional<battery::CoolingPlateProperties> CadDocument::getCoolingPlateProperties(EntityId id) const
{
    const battery::CoolingPlateEntity* plate = findCoolingPlate(id);
    if (plate == nullptr) {
        return std::nullopt;
    }
    return battery::CoolingPlateProperties{summaryFromEntity(*plate), plate->plate_index, plate->center, plate->size};
}

std::optional<battery::ModuleBoundaryProperties> CadDocument::getModuleBoundaryProperties(EntityId id) const
{
    const battery::ModuleBoundaryEntity* boundary = findModuleBoundary(id);
    if (boundary == nullptr) {
        return std::nullopt;
    }
    return battery::ModuleBoundaryProperties{summaryFromEntity(*boundary), boundary->module_index, boundary->center, boundary->size};
}

std::optional<battery::PackEnclosureProperties> CadDocument::getEnclosureProperties(EntityId id) const
{
    const battery::PackEnclosureEntity* enclosure = findEnclosure(id);
    if (enclosure == nullptr) {
        return std::nullopt;
    }
    return battery::PackEnclosureProperties{summaryFromEntity(*enclosure), enclosure->enclosure_index, enclosure->center, enclosure->size, enclosure->wall_thickness};
}

void CadDocument::rebuildIndex()
{
    m_entityIndex.clear();
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
        if constexpr (std::is_same_v<T, battery::CellEntity>) {
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
