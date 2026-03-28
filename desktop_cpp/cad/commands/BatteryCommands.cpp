#include "BatteryCommands.h"

#include "../CadEngine.h"

#include <utility>

namespace cad::commands {

MoveEntityCommand::MoveEntityCommand(core::EntityId entity_id, math::Vec3 delta)
    : m_entityId(entity_id)
    , m_delta(delta)
{
}

bool MoveEntityCommand::redo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        m_previousState = engine.snapshotEntity(m_entityId);
    }
    return engine.moveEntity(m_entityId, m_delta);
}

void MoveEntityCommand::undo(CadEngine& engine)
{
    if (m_previousState.has_value()) {
        engine.restoreEntity(*m_previousState);
    }
}

RemoveEntityCommand::RemoveEntityCommand(core::EntityId entity_id)
    : m_entityId(entity_id)
{
}

bool RemoveEntityCommand::redo(CadEngine& engine)
{
    if (!m_snapshot.has_value()) {
        m_snapshot = engine.snapshotEntity(m_entityId);
        m_previousSelection = engine.selectedEntity();
    }
    if (!m_snapshot.has_value()) {
        return false;
    }
    return engine.removeEntity(m_entityId);
}

void RemoveEntityCommand::undo(CadEngine& engine)
{
    if (!m_snapshot.has_value()) {
        return;
    }
    engine.restoreEntity(*m_snapshot);
    if (m_previousSelection.isValid()) {
        engine.selectEntity(m_previousSelection);
    }
}

UpdateCellGeometryCommand::UpdateCellGeometryCommand(core::EntityId entity_id, float new_radius, float new_height)
    : m_entityId(entity_id)
    , m_newRadius(new_radius)
    , m_newHeight(new_height)
{
}

bool UpdateCellGeometryCommand::redo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        m_previousState = engine.snapshotEntity(m_entityId);
        const auto properties = engine.getCellProperties(m_entityId);
        if (!m_previousState.has_value() || !properties.has_value()) {
            return false;
        }
        m_oldRadius = properties->radius;
        m_oldHeight = properties->height;
        m_capturedInitialState = true;
    }
    return engine.setCellGeometry(m_entityId, m_newRadius, m_newHeight);
}

void UpdateCellGeometryCommand::undo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        return;
    }
    engine.restoreEntity(*m_previousState);
}

RenameEntityCommand::RenameEntityCommand(core::EntityId entity_id, std::string new_label)
    : m_entityId(entity_id)
    , m_newLabel(std::move(new_label))
{
}

bool RenameEntityCommand::redo(CadEngine& engine)
{
    if (!m_capturedInitialState) {
        m_previousState = engine.snapshotEntity(m_entityId);
        const auto summary = engine.getEntitySummary(m_entityId);
        if (!summary.has_value() || !m_previousState.has_value()) {
            return false;
        }
        m_oldLabel = summary->label;
        m_capturedInitialState = true;
    }
    return engine.setEntityLabel(m_entityId, m_newLabel);
}

void RenameEntityCommand::undo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        return;
    }
    engine.restoreEntity(*m_previousState);
}

UpdateCellPropertiesCommand::UpdateCellPropertiesCommand(core::EntityId entity_id, battery::CellPropertiesUpdate update)
    : m_entityId(entity_id)
    , m_update(std::move(update))
{
}

bool UpdateCellPropertiesCommand::redo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        m_previousProperties = engine.getCellProperties(m_entityId);
        if (!m_previousProperties.has_value()) {
            return false;
        }
    }
    return engine.updateCellProperties(m_entityId, m_update);
}

void UpdateCellPropertiesCommand::undo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        return;
    }
    engine.applyCellProperties(m_entityId, *m_previousProperties);
}

UpdateBusbarPropertiesCommand::UpdateBusbarPropertiesCommand(core::EntityId entity_id, battery::BusbarPropertiesUpdate update)
    : m_entityId(entity_id)
    , m_update(std::move(update))
{
}

bool UpdateBusbarPropertiesCommand::redo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        m_previousProperties = engine.getBusbarProperties(m_entityId);
        if (!m_previousProperties.has_value()) {
            return false;
        }
    }
    return engine.updateBusbarProperties(m_entityId, m_update);
}

void UpdateBusbarPropertiesCommand::undo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        return;
    }
    engine.applyBusbarProperties(m_entityId, *m_previousProperties);
}

UpdateCoolingPlatePropertiesCommand::UpdateCoolingPlatePropertiesCommand(core::EntityId entity_id, battery::CoolingPlatePropertiesUpdate update)
    : m_entityId(entity_id)
    , m_update(std::move(update))
{
}

bool UpdateCoolingPlatePropertiesCommand::redo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        m_previousProperties = engine.getCoolingPlateProperties(m_entityId);
        if (!m_previousProperties.has_value()) {
            return false;
        }
    }
    return engine.updateCoolingPlateProperties(m_entityId, m_update);
}

void UpdateCoolingPlatePropertiesCommand::undo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        return;
    }
    engine.applyCoolingPlateProperties(m_entityId, *m_previousProperties);
}

UpdateModuleBoundaryPropertiesCommand::UpdateModuleBoundaryPropertiesCommand(core::EntityId entity_id, battery::ModuleBoundaryPropertiesUpdate update)
    : m_entityId(entity_id)
    , m_update(std::move(update))
{
}

bool UpdateModuleBoundaryPropertiesCommand::redo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        m_previousProperties = engine.getModuleBoundaryProperties(m_entityId);
        if (!m_previousProperties.has_value()) {
            return false;
        }
    }
    return engine.updateModuleBoundaryProperties(m_entityId, m_update);
}

void UpdateModuleBoundaryPropertiesCommand::undo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        return;
    }
    engine.applyModuleBoundaryProperties(m_entityId, *m_previousProperties);
}

UpdateEnclosurePropertiesCommand::UpdateEnclosurePropertiesCommand(core::EntityId entity_id, battery::PackEnclosurePropertiesUpdate update)
    : m_entityId(entity_id)
    , m_update(std::move(update))
{
}

bool UpdateEnclosurePropertiesCommand::redo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        m_previousProperties = engine.getEnclosureProperties(m_entityId);
        if (!m_previousProperties.has_value()) {
            return false;
        }
    }
    return engine.updateEnclosureProperties(m_entityId, m_update);
}

void UpdateEnclosurePropertiesCommand::undo(CadEngine& engine)
{
    if (!m_previousProperties.has_value()) {
        return;
    }
    engine.applyEnclosureProperties(m_entityId, *m_previousProperties);
}

SetEntityVisibilityCommand::SetEntityVisibilityCommand(core::EntityId entity_id, bool visible)
    : m_entityId(entity_id)
    , m_visible(visible)
{
}

bool SetEntityVisibilityCommand::redo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        m_previousState = engine.snapshotEntity(m_entityId);
        if (!m_previousState.has_value()) {
            return false;
        }
    }
    return engine.setEntityVisibility(m_entityId, m_visible);
}

void SetEntityVisibilityCommand::undo(CadEngine& engine)
{
    if (m_previousState.has_value()) {
        engine.restoreEntity(*m_previousState);
    }
}

ResetEntityPositionToGeneratedCommand::ResetEntityPositionToGeneratedCommand(core::EntityId entity_id)
    : m_entityId(entity_id)
{
}

bool ResetEntityPositionToGeneratedCommand::redo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        m_previousState = engine.snapshotEntity(m_entityId);
        if (!m_previousState.has_value()) {
            return false;
        }
    }
    return engine.resetEntityPositionToGenerated(m_entityId);
}

void ResetEntityPositionToGeneratedCommand::undo(CadEngine& engine)
{
    if (m_previousState.has_value()) {
        engine.restoreEntity(*m_previousState);
    }
}

ResetEntityGeometryToGeneratedCommand::ResetEntityGeometryToGeneratedCommand(core::EntityId entity_id)
    : m_entityId(entity_id)
{
}

bool ResetEntityGeometryToGeneratedCommand::redo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        m_previousState = engine.snapshotEntity(m_entityId);
        if (!m_previousState.has_value()) {
            return false;
        }
    }
    return engine.resetEntityGeometryToGenerated(m_entityId);
}

void ResetEntityGeometryToGeneratedCommand::undo(CadEngine& engine)
{
    if (m_previousState.has_value()) {
        engine.restoreEntity(*m_previousState);
    }
}

ResetEntityLabelToGeneratedCommand::ResetEntityLabelToGeneratedCommand(core::EntityId entity_id)
    : m_entityId(entity_id)
{
}

bool ResetEntityLabelToGeneratedCommand::redo(CadEngine& engine)
{
    if (!m_previousState.has_value()) {
        m_previousState = engine.snapshotEntity(m_entityId);
        if (!m_previousState.has_value()) {
            return false;
        }
    }
    return engine.resetEntityLabelToGenerated(m_entityId);
}

void ResetEntityLabelToGeneratedCommand::undo(CadEngine& engine)
{
    if (m_previousState.has_value()) {
        engine.restoreEntity(*m_previousState);
    }
}

} // namespace cad::commands
