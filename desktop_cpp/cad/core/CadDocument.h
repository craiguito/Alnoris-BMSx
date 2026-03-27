#pragma once

#include "../battery/BatteryConfig.h"
#include "../battery/BatteryEntities.h"
#include "SelectionState.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cad::core {

class CadDocument
{
public:
    struct EntityLocator
    {
        battery::EntityKind kind = battery::EntityKind::Cell;
        std::size_t index = 0;
    };

    struct Metadata
    {
        battery::PackLayoutConfig layout_config;
        std::string cell_mesh_path;
    };

    void clear();

    [[nodiscard]] EntityId allocateEntityId();

    battery::CellEntity& addCell(battery::CellEntity entity);
    battery::BusbarEntity& addBusbar(battery::BusbarEntity entity);
    battery::CoolingPlateEntity& addCoolingPlate(battery::CoolingPlateEntity entity);
    battery::ModuleBoundaryEntity& addModuleBoundary(battery::ModuleBoundaryEntity entity);
    battery::PackEnclosureEntity& addPackEnclosure(battery::PackEnclosureEntity entity);

    bool removeEntity(EntityId id);
    bool restoreEntity(const battery::EntityRecord& entity);
    bool moveEntity(EntityId id, const math::Vec3& delta);
    bool setEntityPosition(EntityId id, const math::Vec3& position);
    bool setEntityLabel(EntityId id, std::string label);
    void selectEntity(EntityId id);
    void clearSelection();

    bool setCellPosition(EntityId id, const math::Vec3& position);
    bool setCellGeometry(EntityId id, float radius, float height);
    bool setBusbarGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size);
    bool setCoolingPlateGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size);
    bool setModuleBoundaryGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size);
    bool setEnclosureGeometry(EntityId id, const math::Vec3& center, const math::Vec3& size, float wall_thickness);

    [[nodiscard]] bool hasEntity(EntityId id) const;
    [[nodiscard]] std::optional<battery::EntityKind> entityKind(EntityId id) const;
    [[nodiscard]] battery::CellEntity* findCell(EntityId id);
    [[nodiscard]] const battery::CellEntity* findCell(EntityId id) const;
    [[nodiscard]] battery::BusbarEntity* findBusbar(EntityId id);
    [[nodiscard]] const battery::BusbarEntity* findBusbar(EntityId id) const;
    [[nodiscard]] battery::CoolingPlateEntity* findCoolingPlate(EntityId id);
    [[nodiscard]] const battery::CoolingPlateEntity* findCoolingPlate(EntityId id) const;
    [[nodiscard]] battery::ModuleBoundaryEntity* findModuleBoundary(EntityId id);
    [[nodiscard]] const battery::ModuleBoundaryEntity* findModuleBoundary(EntityId id) const;
    [[nodiscard]] battery::PackEnclosureEntity* findEnclosure(EntityId id);
    [[nodiscard]] const battery::PackEnclosureEntity* findEnclosure(EntityId id) const;

    [[nodiscard]] std::optional<battery::EntityRecord> snapshotEntity(EntityId id) const;
    [[nodiscard]] std::optional<battery::EntitySummary> getEntitySummary(EntityId id) const;
    [[nodiscard]] std::optional<battery::EntitySummary> getSelectedEntitySummary() const;
    [[nodiscard]] std::optional<battery::CellProperties> getCellProperties(EntityId id) const;
    [[nodiscard]] std::optional<battery::BusbarProperties> getBusbarProperties(EntityId id) const;
    [[nodiscard]] std::optional<battery::CoolingPlateProperties> getCoolingPlateProperties(EntityId id) const;
    [[nodiscard]] std::optional<battery::ModuleBoundaryProperties> getModuleBoundaryProperties(EntityId id) const;
    [[nodiscard]] std::optional<battery::PackEnclosureProperties> getEnclosureProperties(EntityId id) const;

    [[nodiscard]] const std::vector<battery::CellEntity>& cells() const { return m_cells; }
    [[nodiscard]] const std::vector<battery::BusbarEntity>& busbars() const { return m_busbars; }
    [[nodiscard]] const std::vector<battery::CoolingPlateEntity>& coolingPlates() const { return m_coolingPlates; }
    [[nodiscard]] const std::vector<battery::ModuleBoundaryEntity>& moduleBoundaries() const { return m_moduleBoundaries; }
    [[nodiscard]] const std::vector<battery::PackEnclosureEntity>& packEnclosures() const { return m_packEnclosures; }

    [[nodiscard]] SelectionState& selection() { return m_selection; }
    [[nodiscard]] const SelectionState& selection() const { return m_selection; }

    [[nodiscard]] Metadata& metadata() { return m_metadata; }
    [[nodiscard]] const Metadata& metadata() const { return m_metadata; }

private:
    void rebuildIndex();
    void indexEntity(EntityId id, battery::EntityKind kind, std::size_t index);

    [[nodiscard]] EntityLocator* findLocator(EntityId id);
    [[nodiscard]] const EntityLocator* findLocator(EntityId id) const;

    std::uint64_t m_nextEntityId = 1;
    Metadata m_metadata;
    SelectionState m_selection;
    std::unordered_map<EntityId, EntityLocator, EntityIdHash> m_entityIndex;
    std::vector<battery::CellEntity> m_cells;
    std::vector<battery::BusbarEntity> m_busbars;
    std::vector<battery::CoolingPlateEntity> m_coolingPlates;
    std::vector<battery::ModuleBoundaryEntity> m_moduleBoundaries;
    std::vector<battery::PackEnclosureEntity> m_packEnclosures;
};

} // namespace cad::core
