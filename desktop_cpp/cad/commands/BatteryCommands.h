#pragma once

#include "../battery/BatteryEntities.h"
#include "../core/EntityId.h"
#include "../math/CadMath.h"
#include "ICommand.h"

#include <optional>
#include <string>
#include <vector>

namespace cad::commands {

struct EntitySubtreeSnapshot
{
    std::vector<battery::EntityRecord> entities;
    core::EntityId previous_selection{};

    [[nodiscard]] bool empty() const { return entities.empty(); }
};

class MoveEntityCommand : public ICommand
{
public:
    MoveEntityCommand(core::EntityId entity_id, math::Vec3 delta);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    math::Vec3 m_delta{};
    std::optional<battery::EntityRecord> m_previousState;
};

class RemoveEntityCommand : public ICommand
{
public:
    explicit RemoveEntityCommand(core::EntityId entity_id);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    std::optional<EntitySubtreeSnapshot> m_snapshot;
};

class UpdateCellGeometryCommand : public ICommand
{
public:
    UpdateCellGeometryCommand(core::EntityId entity_id, float new_radius, float new_height);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    float m_newRadius = 0.0f;
    float m_newHeight = 0.0f;
    float m_oldRadius = 0.0f;
    float m_oldHeight = 0.0f;
    bool m_capturedInitialState = false;
    std::optional<battery::EntityRecord> m_previousState;
};

class RenameEntityCommand : public ICommand
{
public:
    RenameEntityCommand(core::EntityId entity_id, std::string new_label);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    std::string m_newLabel;
    std::string m_oldLabel;
    bool m_capturedInitialState = false;
    std::optional<battery::EntityRecord> m_previousState;
};

class UpdateCellPropertiesCommand : public ICommand
{
public:
    UpdateCellPropertiesCommand(core::EntityId entity_id, battery::CellPropertiesUpdate update);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    battery::CellPropertiesUpdate m_update;
    std::optional<battery::CellProperties> m_previousProperties;
};

class UpdateBusbarPropertiesCommand : public ICommand
{
public:
    UpdateBusbarPropertiesCommand(core::EntityId entity_id, battery::BusbarPropertiesUpdate update);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    battery::BusbarPropertiesUpdate m_update;
    std::optional<battery::BusbarProperties> m_previousProperties;
};

class UpdateCoolingPlatePropertiesCommand : public ICommand
{
public:
    UpdateCoolingPlatePropertiesCommand(core::EntityId entity_id, battery::CoolingPlatePropertiesUpdate update);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    battery::CoolingPlatePropertiesUpdate m_update;
    std::optional<battery::CoolingPlateProperties> m_previousProperties;
};

class UpdateModuleBoundaryPropertiesCommand : public ICommand
{
public:
    UpdateModuleBoundaryPropertiesCommand(core::EntityId entity_id, battery::ModuleBoundaryPropertiesUpdate update);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    battery::ModuleBoundaryPropertiesUpdate m_update;
    std::optional<battery::ModuleBoundaryProperties> m_previousProperties;
};

class UpdateEnclosurePropertiesCommand : public ICommand
{
public:
    UpdateEnclosurePropertiesCommand(core::EntityId entity_id, battery::PackEnclosurePropertiesUpdate update);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    battery::PackEnclosurePropertiesUpdate m_update;
    std::optional<battery::PackEnclosureProperties> m_previousProperties;
};

class SetEntityVisibilityCommand : public ICommand
{
public:
    SetEntityVisibilityCommand(core::EntityId entity_id, bool visible);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    bool m_visible = true;
    std::optional<battery::EntityRecord> m_previousState;
};

class ResetEntityPositionToGeneratedCommand : public ICommand
{
public:
    explicit ResetEntityPositionToGeneratedCommand(core::EntityId entity_id);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    std::optional<battery::EntityRecord> m_previousState;
};

class ResetEntityGeometryToGeneratedCommand : public ICommand
{
public:
    explicit ResetEntityGeometryToGeneratedCommand(core::EntityId entity_id);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    std::optional<battery::EntityRecord> m_previousState;
};

class ResetEntityLabelToGeneratedCommand : public ICommand
{
public:
    explicit ResetEntityLabelToGeneratedCommand(core::EntityId entity_id);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    std::optional<battery::EntityRecord> m_previousState;
};

} // namespace cad::commands
