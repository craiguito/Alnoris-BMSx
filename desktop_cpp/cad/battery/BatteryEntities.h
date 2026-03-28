#pragma once

#include "../core/EntityId.h"
#include "../math/CadMath.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace cad::battery {

enum class EntityKind
{
    BatteryPack,
    BatteryModule,
    CellGroup,
    Cell,
    Busbar,
    CoolingChannel,
    Enclosure,

    ModuleBoundary = BatteryModule,
    CoolingPlate = CoolingChannel,
    PackEnclosure = Enclosure
};

enum class BusbarRole
{
    Negative,
    Positive
};

enum class LayoutType
{
    Grid
};

enum class PropertyMode
{
    Generated,
    UserOverride
};

struct BoundingBox
{
    math::Vec3 center{};
    math::Vec3 size{};
};

struct EntityPropertyModes
{
    PropertyMode position = PropertyMode::Generated;
    PropertyMode geometry = PropertyMode::Generated;
    PropertyMode label = PropertyMode::Generated;
    PropertyMode visibility = PropertyMode::Generated;
};

struct CadEntity
{
    core::EntityId id{};
    core::EntityId parent_id{};
    EntityKind kind = EntityKind::Cell;
    std::string label;
    bool visible = true;
    bool selectable = true;
    EntityPropertyModes property_modes;
    int simulation_group_index = -1;

    [[nodiscard]] bool hasParent() const { return parent_id.isValid(); }
};

struct BatteryPack : CadEntity
{
    math::Vec3 center{};
    math::Vec3 size{760.0f, 320.0f, 420.0f};
    int series_count = 0;
    int parallel_count = 0;
    float cell_radius = 0.0f;
    float cell_height = 0.0f;
    float spacing_x = 0.0f;
    float spacing_z = 0.0f;
    LayoutType layout_type = LayoutType::Grid;

    [[nodiscard]] BoundingBox localBounds() const { return {center, size}; }
};

struct BatteryModule : CadEntity
{
    int module_index = 0;
    math::Vec3 center{};
    math::Vec3 size{720.0f, 280.0f, 380.0f};
    int series_span = 0;
    int parallel_span = 0;

    [[nodiscard]] BoundingBox localBounds() const { return {center, size}; }
};

struct CellGroup : CadEntity
{
    int group_index = 0;
    int series_index = 0;
    math::Vec3 center{};
    math::Vec3 size{96.0f, 260.0f, 300.0f};
    int cell_count = 0;

    [[nodiscard]] BoundingBox localBounds() const { return {center, size}; }
};

struct BatteryCell : CadEntity
{
    math::Vec3 position{};
    float radius = 28.0f;
    float height = 220.0f;
    int series_index = 0;
    int parallel_index = 0;
    std::string cell_type = "18650";

    [[nodiscard]] BoundingBox localBounds() const
    {
        return {position, {radius * 2.0f, height, radius * 2.0f}};
    }
};

struct Busbar : CadEntity
{
    BusbarRole role = BusbarRole::Negative;
    math::Vec3 center{};
    math::Vec3 size{700.0f, 12.0f, 16.0f};

    [[nodiscard]] BoundingBox localBounds() const { return {center, size}; }
};

struct CoolingChannel : CadEntity
{
    int plate_index = 0;
    math::Vec3 center{};
    math::Vec3 size{680.0f, 24.0f, 320.0f};

    [[nodiscard]] BoundingBox localBounds() const { return {center, size}; }
};

struct Enclosure : CadEntity
{
    int enclosure_index = 0;
    math::Vec3 center{};
    math::Vec3 size{760.0f, 320.0f, 420.0f};
    float wall_thickness = 8.0f;

    [[nodiscard]] BoundingBox localBounds() const { return {center, size}; }
};

using CellEntity = BatteryCell;
using BusbarEntity = Busbar;
using CoolingPlateEntity = CoolingChannel;
using ModuleBoundaryEntity = BatteryModule;
using PackEnclosureEntity = Enclosure;
using BatteryPackEntity = BatteryPack;
using CellGroupEntity = CellGroup;

using EntityRecord = std::variant<
    BatteryPackEntity,
    BatteryModule,
    CellGroupEntity,
    CellEntity,
    BusbarEntity,
    CoolingPlateEntity,
    PackEnclosureEntity
>;

struct EntitySummary
{
    core::EntityId id{};
    EntityKind kind = EntityKind::Cell;
    std::string label;
    bool visible = true;
    core::EntityId parent_id{};
    int simulation_group_index = -1;
};

struct CellProperties
{
    EntitySummary summary;
    math::Vec3 position{};
    float radius = 0.0f;
    float height = 0.0f;
    int series_index = 0;
    int parallel_index = 0;
    EntityPropertyModes property_modes;
};

struct BusbarProperties
{
    EntitySummary summary;
    BusbarRole role = BusbarRole::Negative;
    math::Vec3 center{};
    math::Vec3 size{};
    EntityPropertyModes property_modes;
};

struct CoolingPlateProperties
{
    EntitySummary summary;
    int plate_index = 0;
    math::Vec3 center{};
    math::Vec3 size{};
    EntityPropertyModes property_modes;
};

struct ModuleBoundaryProperties
{
    EntitySummary summary;
    int module_index = 0;
    math::Vec3 center{};
    math::Vec3 size{};
    EntityPropertyModes property_modes;
};

struct PackEnclosureProperties
{
    EntitySummary summary;
    int enclosure_index = 0;
    math::Vec3 center{};
    math::Vec3 size{};
    float wall_thickness = 0.0f;
    EntityPropertyModes property_modes;
};

struct CellPropertiesUpdate
{
    std::optional<math::Vec3> position;
    std::optional<float> radius;
    std::optional<float> height;
    std::optional<std::string> label;
    std::optional<bool> visible;
};

struct BusbarPropertiesUpdate
{
    std::optional<math::Vec3> center;
    std::optional<math::Vec3> size;
    std::optional<std::string> label;
    std::optional<bool> visible;
};

struct CoolingPlatePropertiesUpdate
{
    std::optional<math::Vec3> center;
    std::optional<math::Vec3> size;
    std::optional<std::string> label;
    std::optional<bool> visible;
};

struct ModuleBoundaryPropertiesUpdate
{
    std::optional<math::Vec3> center;
    std::optional<math::Vec3> size;
    std::optional<std::string> label;
    std::optional<bool> visible;
};

struct PackEnclosurePropertiesUpdate
{
    std::optional<math::Vec3> center;
    std::optional<math::Vec3> size;
    std::optional<float> wall_thickness;
    std::optional<std::string> label;
    std::optional<bool> visible;
};

struct BatteryVisualizationOverlay
{
    enum class Metric
    {
        CoreTemperature,
        SurfaceTemperature,
        Soc,
        Voltage,
        DiffusionStress,
        EffectiveResistance
    };

    Metric active_metric = Metric::CoreTemperature;
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_core_temperature_c;
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_surface_temperature_c;
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_soc;
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_voltage_v;
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_diffusion_stress;
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_effective_resistance_ohm;
};

} // namespace cad::battery
