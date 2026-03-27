#pragma once

#include "../core/EntityId.h"
#include "../math/CadMath.h"

#include <string>
#include <optional>
#include <unordered_map>
#include <variant>

namespace cad::battery {

enum class EntityKind
{
    Cell,
    Busbar,
    CoolingPlate,
    ModuleBoundary,
    PackEnclosure
};

enum class BusbarRole
{
    Negative,
    Positive
};

enum class PropertyMode
{
    Generated,
    UserOverride
};

struct EntityPropertyModes
{
    PropertyMode position = PropertyMode::Generated;
    PropertyMode geometry = PropertyMode::Generated;
    PropertyMode label = PropertyMode::Generated;
    PropertyMode visibility = PropertyMode::Generated;
};

struct EntityBase
{
    core::EntityId id{};
    EntityKind kind = EntityKind::Cell;
    std::string label;
    bool visible = true;
    EntityPropertyModes property_modes;
};

struct CellEntity : EntityBase
{
    math::Vec3 position{};
    float radius = 28.0f;
    float height = 220.0f;
    int series_index = 0;
    int parallel_index = 0;
};

struct BusbarEntity : EntityBase
{
    BusbarRole role = BusbarRole::Negative;
    math::Vec3 center{};
    math::Vec3 size{700.0f, 12.0f, 16.0f};
};

struct CoolingPlateEntity : EntityBase
{
    int plate_index = 0;
    math::Vec3 center{};
    math::Vec3 size{680.0f, 24.0f, 320.0f};
};

struct ModuleBoundaryEntity : EntityBase
{
    int module_index = 0;
    math::Vec3 center{};
    math::Vec3 size{720.0f, 280.0f, 380.0f};
};

struct PackEnclosureEntity : EntityBase
{
    int enclosure_index = 0;
    math::Vec3 center{};
    math::Vec3 size{760.0f, 320.0f, 420.0f};
    float wall_thickness = 8.0f;
};

using EntityRecord = std::variant<CellEntity, BusbarEntity, CoolingPlateEntity, ModuleBoundaryEntity, PackEnclosureEntity>;

struct EntitySummary
{
    core::EntityId id{};
    EntityKind kind = EntityKind::Cell;
    std::string label;
    bool visible = true;
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
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_temperature_c;
};

} // namespace cad::battery
