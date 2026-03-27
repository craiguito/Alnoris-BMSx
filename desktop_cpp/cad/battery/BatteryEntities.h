#pragma once

#include "../core/EntityId.h"
#include "../math/CadMath.h"

#include <string>
#include <unordered_map>

namespace cad::battery {

enum class EntityKind
{
    Cell,
    Busbar,
    CoolingPlate,
    ModuleBoundary,
    PackEnclosure
};

struct EntityBase
{
    core::EntityId id{};
    EntityKind kind = EntityKind::Cell;
    std::string label;
    bool visible = true;
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
    math::Vec3 center{};
    math::Vec3 size{700.0f, 12.0f, 16.0f};
};

struct CoolingPlateEntity : EntityBase
{
    math::Vec3 center{};
    math::Vec3 size{680.0f, 24.0f, 320.0f};
};

struct ModuleBoundaryEntity : EntityBase
{
    math::Vec3 center{};
    math::Vec3 size{720.0f, 280.0f, 380.0f};
};

struct PackEnclosureEntity : EntityBase
{
    math::Vec3 center{};
    math::Vec3 size{760.0f, 320.0f, 420.0f};
    float wall_thickness = 8.0f;
};

struct BatteryVisualizationOverlay
{
    std::unordered_map<core::EntityId, double, core::EntityIdHash> cell_temperature_c;
};

} // namespace cad::battery
