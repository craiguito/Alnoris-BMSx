#pragma once

#include "../battery/BatteryConfig.h"
#include "../battery/BatteryEntities.h"
#include "SelectionState.h"

#include <string>
#include <vector>

namespace cad::core {

class CadDocument
{
public:
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

    [[nodiscard]] bool hasEntity(EntityId id) const;

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
    std::uint64_t m_nextEntityId = 1;
    Metadata m_metadata;
    SelectionState m_selection;
    std::vector<battery::CellEntity> m_cells;
    std::vector<battery::BusbarEntity> m_busbars;
    std::vector<battery::CoolingPlateEntity> m_coolingPlates;
    std::vector<battery::ModuleBoundaryEntity> m_moduleBoundaries;
    std::vector<battery::PackEnclosureEntity> m_packEnclosures;
};

} // namespace cad::core
