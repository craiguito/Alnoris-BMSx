#pragma once

#include "../battery/BatteryEntities.h"
#include "../core/EntityId.h"
#include "../math/CadMath.h"
#include "ICommand.h"

#include <optional>
#include <string>

namespace cad::commands {

class MoveEntityCommand : public ICommand
{
public:
    MoveEntityCommand(core::EntityId entity_id, math::Vec3 delta);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    math::Vec3 m_delta{};
};

class RemoveEntityCommand : public ICommand
{
public:
    explicit RemoveEntityCommand(core::EntityId entity_id);

    bool redo(CadEngine& engine) override;
    void undo(CadEngine& engine) override;

private:
    core::EntityId m_entityId{};
    core::EntityId m_previousSelection{};
    std::optional<battery::EntityRecord> m_snapshot;
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

} // namespace cad::commands
