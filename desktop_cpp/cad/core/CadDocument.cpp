#include "CadDocument.h"

namespace cad::core {

void CadDocument::clear()
{
    m_selection.clear();
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
    return m_cells.back();
}

battery::BusbarEntity& CadDocument::addBusbar(battery::BusbarEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::Busbar;
    m_busbars.push_back(std::move(entity));
    return m_busbars.back();
}

battery::CoolingPlateEntity& CadDocument::addCoolingPlate(battery::CoolingPlateEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::CoolingPlate;
    m_coolingPlates.push_back(std::move(entity));
    return m_coolingPlates.back();
}

battery::ModuleBoundaryEntity& CadDocument::addModuleBoundary(battery::ModuleBoundaryEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::ModuleBoundary;
    m_moduleBoundaries.push_back(std::move(entity));
    return m_moduleBoundaries.back();
}

battery::PackEnclosureEntity& CadDocument::addPackEnclosure(battery::PackEnclosureEntity entity)
{
    if (!entity.id.isValid()) {
        entity.id = allocateEntityId();
    }
    entity.kind = battery::EntityKind::PackEnclosure;
    m_packEnclosures.push_back(std::move(entity));
    return m_packEnclosures.back();
}

bool CadDocument::hasEntity(EntityId id) const
{
    if (!id.isValid()) {
        return false;
    }

    const auto hasId = [id](const auto& entities) {
        for (const auto& entity : entities) {
            if (entity.id == id) {
                return true;
            }
        }
        return false;
    };

    return hasId(m_cells)
        || hasId(m_busbars)
        || hasId(m_coolingPlates)
        || hasId(m_moduleBoundaries)
        || hasId(m_packEnclosures);
}

} // namespace cad::core
