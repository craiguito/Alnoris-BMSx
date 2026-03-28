#pragma once

#include "../battery/BatteryConfig.h"
#include "../battery/BatteryEntities.h"
#include "../core/CadDocument.h"

namespace cad::edit {

class CadEditService
{
public:
    static bool moveEntity(core::CadDocument& document, core::EntityId entity_id, const math::Vec3& delta);
    static bool setEntityLabel(core::CadDocument& document, core::EntityId entity_id, std::string label);
    static bool setEntityVisibility(core::CadDocument& document, core::EntityId entity_id, bool visible);
    static bool updateCellProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CellPropertiesUpdate& update);
    static bool updateBusbarProperties(core::CadDocument& document, core::EntityId entity_id, const battery::BusbarPropertiesUpdate& update);
    static bool updateCoolingPlateProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CoolingPlatePropertiesUpdate& update);
    static bool updateModuleBoundaryProperties(core::CadDocument& document, core::EntityId entity_id, const battery::ModuleBoundaryPropertiesUpdate& update);
    static bool updateEnclosureProperties(core::CadDocument& document, core::EntityId entity_id, const battery::PackEnclosurePropertiesUpdate& update);
    static bool applyCellProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CellProperties& properties);
    static bool applyBusbarProperties(core::CadDocument& document, core::EntityId entity_id, const battery::BusbarProperties& properties);
    static bool applyCoolingPlateProperties(core::CadDocument& document, core::EntityId entity_id, const battery::CoolingPlateProperties& properties);
    static bool applyModuleBoundaryProperties(core::CadDocument& document, core::EntityId entity_id, const battery::ModuleBoundaryProperties& properties);
    static bool applyEnclosureProperties(core::CadDocument& document, core::EntityId entity_id, const battery::PackEnclosureProperties& properties);
    static bool resetEntityPositionToGenerated(core::CadDocument& document, const battery::PackLayoutConfig& layout, core::EntityId entity_id);
    static bool resetEntityGeometryToGenerated(core::CadDocument& document, const battery::PackLayoutConfig& layout, core::EntityId entity_id);
    static bool resetEntityLabelToGenerated(core::CadDocument& document, core::EntityId entity_id);
};

} // namespace cad::edit
