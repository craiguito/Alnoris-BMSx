#include "JsonCadDocumentIO.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <cstdint>

namespace cad::io {
namespace {

constexpr int kCadDocumentVersion = 1;

using cad::battery::BatteryPackEntity;
using cad::battery::BusbarEntity;
using cad::battery::CellEntity;
using cad::battery::CellGroupEntity;
using cad::battery::CoolingPlateEntity;
using cad::battery::EntityPropertyModes;
using cad::battery::LayoutType;
using cad::battery::ModuleBoundaryEntity;
using cad::battery::PackEnclosureEntity;
using cad::battery::PackLayoutConfig;
using cad::battery::PropertyMode;
using cad::math::Vec3;
using cad::core::EntityId;

QString entityIdToString(EntityId entity_id)
{
    return entity_id.isValid() ? QString::number(entity_id.value) : QString();
}

EntityId entityIdFromJson(const QJsonValue& value)
{
    if (value.isString()) {
        return EntityId{value.toString().toULongLong()};
    }
    if (value.isDouble()) {
        return EntityId{static_cast<std::uint64_t>(value.toDouble())};
    }
    return {};
}

QJsonObject vec3ToJson(const Vec3& value)
{
    QJsonObject object;
    object.insert("x", value.x);
    object.insert("y", value.y);
    object.insert("z", value.z);
    return object;
}

Vec3 vec3FromJson(const QJsonValue& value, const Vec3& fallback = {})
{
    if (!value.isObject()) {
        return fallback;
    }

    const QJsonObject object = value.toObject();
    return {
        static_cast<float>(object.value("x").toDouble(fallback.x)),
        static_cast<float>(object.value("y").toDouble(fallback.y)),
        static_cast<float>(object.value("z").toDouble(fallback.z))
    };
}

QString propertyModeToString(PropertyMode mode)
{
    return mode == PropertyMode::UserOverride ? "user_override" : "generated";
}

PropertyMode propertyModeFromJson(const QJsonValue& value, PropertyMode fallback = PropertyMode::Generated)
{
    if (!value.isString()) {
        return fallback;
    }

    return value.toString() == "user_override" ? PropertyMode::UserOverride : PropertyMode::Generated;
}

QJsonObject propertyModesToJson(const EntityPropertyModes& modes)
{
    QJsonObject object;
    object.insert("position", propertyModeToString(modes.position));
    object.insert("geometry", propertyModeToString(modes.geometry));
    object.insert("label", propertyModeToString(modes.label));
    object.insert("visibility", propertyModeToString(modes.visibility));
    return object;
}

EntityPropertyModes propertyModesFromJson(const QJsonValue& value, const EntityPropertyModes& fallback = {})
{
    EntityPropertyModes modes = fallback;
    if (!value.isObject()) {
        return modes;
    }

    const QJsonObject object = value.toObject();
    modes.position = propertyModeFromJson(object.value("position"), modes.position);
    modes.geometry = propertyModeFromJson(object.value("geometry"), modes.geometry);
    modes.label = propertyModeFromJson(object.value("label"), modes.label);
    modes.visibility = propertyModeFromJson(object.value("visibility"), modes.visibility);
    return modes;
}

QString cellFormFactorToString(cad::battery::CellFormFactor form_factor)
{
    switch (form_factor) {
    case cad::battery::CellFormFactor::Prismatic:
        return "prismatic";
    case cad::battery::CellFormFactor::Pouch:
        return "pouch";
    case cad::battery::CellFormFactor::Cylindrical:
    default:
        return "cylindrical";
    }
}

cad::battery::CellFormFactor cellFormFactorFromJson(
    const QJsonValue& value,
    cad::battery::CellFormFactor fallback = cad::battery::CellFormFactor::Cylindrical)
{
    if (!value.isString()) {
        return fallback;
    }

    const QString normalized = value.toString().trimmed().toLower();
    if (normalized == "prismatic") {
        return cad::battery::CellFormFactor::Prismatic;
    }
    if (normalized == "pouch") {
        return cad::battery::CellFormFactor::Pouch;
    }
    return cad::battery::CellFormFactor::Cylindrical;
}

QString busbarRoleToString(cad::battery::BusbarRole role)
{
    return role == cad::battery::BusbarRole::Positive ? "positive" : "negative";
}

cad::battery::BusbarRole busbarRoleFromJson(
    const QJsonValue& value,
    cad::battery::BusbarRole fallback = cad::battery::BusbarRole::Negative)
{
    if (!value.isString()) {
        return fallback;
    }
    return value.toString() == "positive" ? cad::battery::BusbarRole::Positive : cad::battery::BusbarRole::Negative;
}

QString layoutTypeToString(LayoutType layout_type)
{
    switch (layout_type) {
    case LayoutType::Grid:
    default:
        return "grid";
    }
}

LayoutType layoutTypeFromJson(const QJsonValue& value, LayoutType fallback = LayoutType::Grid)
{
    if (!value.isString()) {
        return fallback;
    }
    return value.toString() == "grid" ? LayoutType::Grid : fallback;
}

QJsonObject serializeBaseEntity(const cad::battery::CadEntity& entity)
{
    QJsonObject object;
    object.insert("id", entityIdToString(entity.id));
    object.insert("parent_id", entityIdToString(entity.parent_id));
    object.insert("label", QString::fromStdString(entity.label));
    object.insert("visible", entity.visible);
    object.insert("selectable", entity.selectable);
    object.insert("simulation_group_index", entity.simulation_group_index);
    object.insert("property_modes", propertyModesToJson(entity.property_modes));
    return object;
}

template <typename T>
void deserializeBaseEntity(const QJsonObject& object, T& entity, cad::battery::EntityKind kind)
{
    entity.kind = kind;
    entity.id = entityIdFromJson(object.value("id"));
    entity.parent_id = entityIdFromJson(object.value("parent_id"));
    entity.label = object.value("label").toString(QString::fromStdString(entity.label)).toStdString();
    entity.visible = object.value("visible").toBool(entity.visible);
    entity.selectable = object.value("selectable").toBool(entity.selectable);
    entity.simulation_group_index = object.value("simulation_group_index").toInt(entity.simulation_group_index);
    entity.property_modes = propertyModesFromJson(object.value("property_modes"), entity.property_modes);
}

QJsonObject serializeLayoutConfig(const PackLayoutConfig& config)
{
    QJsonObject object;
    object.insert("preset_name", QString::fromStdString(config.preset_name));
    object.insert("cells_in_series", config.cells_in_series);
    object.insert("cells_in_parallel", config.cells_in_parallel);
    object.insert("module_count", config.module_count);
    object.insert("cell_form_factor", cellFormFactorToString(config.cell_form_factor));
    object.insert("cell_radius", config.cell_radius);
    object.insert("cell_height", config.cell_height);
    object.insert("cell_width", config.cell_width);
    object.insert("cell_depth", config.cell_depth);
    object.insert("top_cap_outer_diameter", config.top_cap_outer_diameter);
    object.insert("top_cap_inner_diameter", config.top_cap_inner_diameter);
    object.insert("top_cap_shoulder_height", config.top_cap_shoulder_height);
    object.insert("positive_terminal_diameter", config.positive_terminal_diameter);
    object.insert("positive_terminal_height", config.positive_terminal_height);
    object.insert("insulating_ring_outer_diameter", config.insulating_ring_outer_diameter);
    object.insert("insulating_ring_inner_diameter", config.insulating_ring_inner_diameter);
    object.insert("insulating_ring_height", config.insulating_ring_height);
    object.insert("bottom_cap_height", config.bottom_cap_height);
    object.insert("x_spacing", config.x_spacing);
    object.insert("z_spacing", config.z_spacing);
    object.insert("module_gap_x", config.module_gap_x);
    object.insert("busbar_thickness", config.busbar_thickness);
    object.insert("busbar_width", config.busbar_width);
    object.insert("busbar_terminal_clearance", config.busbar_terminal_clearance);
    object.insert("busbar_support_offset", config.busbar_support_offset);
    object.insert("busbar_overlap_width", config.busbar_overlap_width);
    object.insert("busbar_tab_width", config.busbar_tab_width);
    object.insert("busbar_tab_depth", config.busbar_tab_depth);
    object.insert("cooling_channel_thickness", config.cooling_channel_thickness);
    object.insert("cooling_channel_depth", config.cooling_channel_depth);
    object.insert("cooling_plate_margin_x", config.cooling_plate_margin_x);
    object.insert("cooling_plate_margin_z", config.cooling_plate_margin_z);
    object.insert("cooling_plate_offset_below_tray", config.cooling_plate_offset_below_tray);
    object.insert("enclosure_wall_thickness", config.enclosure_wall_thickness);
    object.insert("enclosure_floor_thickness", config.enclosure_floor_thickness);
    object.insert("enclosure_floor_offset", config.enclosure_floor_offset);
    object.insert("enclosure_clearance_x", config.enclosure_clearance_x);
    object.insert("enclosure_clearance_z", config.enclosure_clearance_z);
    object.insert("module_tray_base_thickness", config.module_tray_base_thickness);
    object.insert("module_tray_wall_thickness", config.module_tray_wall_thickness);
    object.insert("module_tray_wall_height", config.module_tray_wall_height);
    object.insert("cell_seating_offset", config.cell_seating_offset);
    object.insert("support_rib_thickness", config.support_rib_thickness);
    object.insert("support_rib_height", config.support_rib_height);
    object.insert("module_tray_margin_x", config.module_tray_margin_x);
    object.insert("module_tray_margin_z", config.module_tray_margin_z);
    return object;
}

PackLayoutConfig deserializeLayoutConfig(const QJsonObject& object)
{
    PackLayoutConfig config;
    config.preset_name = object.value("preset_name").toString(QString::fromStdString(config.preset_name)).toStdString();
    config.cells_in_series = object.value("cells_in_series").toInt(config.cells_in_series);
    config.cells_in_parallel = object.value("cells_in_parallel").toInt(config.cells_in_parallel);
    config.module_count = object.value("module_count").toInt(config.module_count);
    config.cell_form_factor = cellFormFactorFromJson(object.value("cell_form_factor"), config.cell_form_factor);
    config.cell_radius = static_cast<float>(object.value("cell_radius").toDouble(config.cell_radius));
    config.cell_height = static_cast<float>(object.value("cell_height").toDouble(config.cell_height));
    config.cell_width = static_cast<float>(object.value("cell_width").toDouble(config.cell_width));
    config.cell_depth = static_cast<float>(object.value("cell_depth").toDouble(config.cell_depth));
    config.top_cap_outer_diameter = static_cast<float>(object.value("top_cap_outer_diameter").toDouble(config.top_cap_outer_diameter));
    config.top_cap_inner_diameter = static_cast<float>(object.value("top_cap_inner_diameter").toDouble(config.top_cap_inner_diameter));
    config.top_cap_shoulder_height = static_cast<float>(object.value("top_cap_shoulder_height").toDouble(config.top_cap_shoulder_height));
    config.positive_terminal_diameter = static_cast<float>(object.value("positive_terminal_diameter").toDouble(config.positive_terminal_diameter));
    config.positive_terminal_height = static_cast<float>(object.value("positive_terminal_height").toDouble(config.positive_terminal_height));
    config.insulating_ring_outer_diameter = static_cast<float>(object.value("insulating_ring_outer_diameter").toDouble(config.insulating_ring_outer_diameter));
    config.insulating_ring_inner_diameter = static_cast<float>(object.value("insulating_ring_inner_diameter").toDouble(config.insulating_ring_inner_diameter));
    config.insulating_ring_height = static_cast<float>(object.value("insulating_ring_height").toDouble(config.insulating_ring_height));
    config.bottom_cap_height = static_cast<float>(object.value("bottom_cap_height").toDouble(config.bottom_cap_height));
    config.x_spacing = static_cast<float>(object.value("x_spacing").toDouble(config.x_spacing));
    config.z_spacing = static_cast<float>(object.value("z_spacing").toDouble(config.z_spacing));
    config.module_gap_x = static_cast<float>(object.value("module_gap_x").toDouble(config.module_gap_x));
    config.busbar_thickness = static_cast<float>(object.value("busbar_thickness").toDouble(config.busbar_thickness));
    config.busbar_width = static_cast<float>(object.value("busbar_width").toDouble(config.busbar_width));
    config.busbar_terminal_clearance = static_cast<float>(object.value("busbar_terminal_clearance").toDouble(config.busbar_terminal_clearance));
    config.busbar_support_offset = static_cast<float>(object.value("busbar_support_offset").toDouble(config.busbar_support_offset));
    config.busbar_overlap_width = static_cast<float>(object.value("busbar_overlap_width").toDouble(config.busbar_overlap_width));
    config.busbar_tab_width = static_cast<float>(object.value("busbar_tab_width").toDouble(config.busbar_tab_width));
    config.busbar_tab_depth = static_cast<float>(object.value("busbar_tab_depth").toDouble(config.busbar_tab_depth));
    config.cooling_channel_thickness = static_cast<float>(object.value("cooling_channel_thickness").toDouble(config.cooling_channel_thickness));
    config.cooling_channel_depth = static_cast<float>(object.value("cooling_channel_depth").toDouble(config.cooling_channel_depth));
    config.cooling_plate_margin_x = static_cast<float>(object.value("cooling_plate_margin_x").toDouble(config.cooling_plate_margin_x));
    config.cooling_plate_margin_z = static_cast<float>(object.value("cooling_plate_margin_z").toDouble(config.cooling_plate_margin_z));
    config.cooling_plate_offset_below_tray = static_cast<float>(object.value("cooling_plate_offset_below_tray").toDouble(config.cooling_plate_offset_below_tray));
    config.enclosure_wall_thickness = static_cast<float>(object.value("enclosure_wall_thickness").toDouble(config.enclosure_wall_thickness));
    config.enclosure_floor_thickness = static_cast<float>(object.value("enclosure_floor_thickness").toDouble(config.enclosure_floor_thickness));
    config.enclosure_floor_offset = static_cast<float>(object.value("enclosure_floor_offset").toDouble(config.enclosure_floor_offset));
    config.enclosure_clearance_x = static_cast<float>(object.value("enclosure_clearance_x").toDouble(config.enclosure_clearance_x));
    config.enclosure_clearance_z = static_cast<float>(object.value("enclosure_clearance_z").toDouble(config.enclosure_clearance_z));
    config.module_tray_base_thickness = static_cast<float>(object.value("module_tray_base_thickness").toDouble(config.module_tray_base_thickness));
    config.module_tray_wall_thickness = static_cast<float>(object.value("module_tray_wall_thickness").toDouble(config.module_tray_wall_thickness));
    config.module_tray_wall_height = static_cast<float>(object.value("module_tray_wall_height").toDouble(config.module_tray_wall_height));
    config.cell_seating_offset = static_cast<float>(object.value("cell_seating_offset").toDouble(config.cell_seating_offset));
    config.support_rib_thickness = static_cast<float>(object.value("support_rib_thickness").toDouble(config.support_rib_thickness));
    config.support_rib_height = static_cast<float>(object.value("support_rib_height").toDouble(config.support_rib_height));
    config.module_tray_margin_x = static_cast<float>(object.value("module_tray_margin_x").toDouble(config.module_tray_margin_x));
    config.module_tray_margin_z = static_cast<float>(object.value("module_tray_margin_z").toDouble(config.module_tray_margin_z));
    return config;
}

QJsonObject serializeMetadata(const cad::core::CadDocument::Metadata& metadata)
{
    QJsonObject object;
    object.insert("layout_config", serializeLayoutConfig(metadata.layout_config));
    object.insert("cell_mesh_path", QString::fromStdString(metadata.cell_mesh_path));
    return object;
}

cad::core::CadDocument::Metadata deserializeMetadata(const QJsonObject& object)
{
    cad::core::CadDocument::Metadata metadata;
    metadata.layout_config = deserializeLayoutConfig(object.value("layout_config").toObject());
    metadata.cell_mesh_path = object.value("cell_mesh_path").toString().toStdString();
    return metadata;
}

QJsonObject serializePack(const BatteryPackEntity& entity)
{
    QJsonObject object = serializeBaseEntity(entity);
    object.insert("center", vec3ToJson(entity.center));
    object.insert("size", vec3ToJson(entity.size));
    object.insert("series_count", entity.series_count);
    object.insert("parallel_count", entity.parallel_count);
    object.insert("cell_radius", entity.cell_radius);
    object.insert("cell_height", entity.cell_height);
    object.insert("spacing_x", entity.spacing_x);
    object.insert("spacing_z", entity.spacing_z);
    object.insert("layout_type", layoutTypeToString(entity.layout_type));
    return object;
}

BatteryPackEntity deserializePack(const QJsonObject& object)
{
    BatteryPackEntity entity;
    deserializeBaseEntity(object, entity, cad::battery::EntityKind::BatteryPack);
    entity.center = vec3FromJson(object.value("center"), entity.center);
    entity.size = vec3FromJson(object.value("size"), entity.size);
    entity.series_count = object.value("series_count").toInt(entity.series_count);
    entity.parallel_count = object.value("parallel_count").toInt(entity.parallel_count);
    entity.cell_radius = static_cast<float>(object.value("cell_radius").toDouble(entity.cell_radius));
    entity.cell_height = static_cast<float>(object.value("cell_height").toDouble(entity.cell_height));
    entity.spacing_x = static_cast<float>(object.value("spacing_x").toDouble(entity.spacing_x));
    entity.spacing_z = static_cast<float>(object.value("spacing_z").toDouble(entity.spacing_z));
    entity.layout_type = layoutTypeFromJson(object.value("layout_type"), entity.layout_type);
    return entity;
}

QJsonObject serializeModule(const ModuleBoundaryEntity& entity)
{
    QJsonObject object = serializeBaseEntity(entity);
    object.insert("module_index", entity.module_index);
    object.insert("center", vec3ToJson(entity.center));
    object.insert("size", vec3ToJson(entity.size));
    object.insert("series_span", entity.series_span);
    object.insert("parallel_span", entity.parallel_span);
    return object;
}

ModuleBoundaryEntity deserializeModule(const QJsonObject& object)
{
    ModuleBoundaryEntity entity;
    deserializeBaseEntity(object, entity, cad::battery::EntityKind::ModuleBoundary);
    entity.module_index = object.value("module_index").toInt(entity.module_index);
    entity.center = vec3FromJson(object.value("center"), entity.center);
    entity.size = vec3FromJson(object.value("size"), entity.size);
    entity.series_span = object.value("series_span").toInt(entity.series_span);
    entity.parallel_span = object.value("parallel_span").toInt(entity.parallel_span);
    return entity;
}

QJsonObject serializeGroup(const CellGroupEntity& entity)
{
    QJsonObject object = serializeBaseEntity(entity);
    object.insert("group_index", entity.group_index);
    object.insert("series_index", entity.series_index);
    object.insert("center", vec3ToJson(entity.center));
    object.insert("size", vec3ToJson(entity.size));
    object.insert("cell_count", entity.cell_count);
    return object;
}

CellGroupEntity deserializeGroup(const QJsonObject& object)
{
    CellGroupEntity entity;
    deserializeBaseEntity(object, entity, cad::battery::EntityKind::CellGroup);
    entity.group_index = object.value("group_index").toInt(entity.group_index);
    entity.series_index = object.value("series_index").toInt(entity.series_index);
    entity.center = vec3FromJson(object.value("center"), entity.center);
    entity.size = vec3FromJson(object.value("size"), entity.size);
    entity.cell_count = object.value("cell_count").toInt(entity.cell_count);
    return entity;
}

QJsonObject serializeCell(const CellEntity& entity)
{
    QJsonObject object = serializeBaseEntity(entity);
    object.insert("position", vec3ToJson(entity.position));
    object.insert("form_factor", cellFormFactorToString(entity.form_factor));
    object.insert("radius", entity.radius);
    object.insert("height", entity.height);
    object.insert("width", entity.width);
    object.insert("depth", entity.depth);
    object.insert("series_index", entity.series_index);
    object.insert("parallel_index", entity.parallel_index);
    object.insert("cell_type", QString::fromStdString(entity.cell_type));
    return object;
}

CellEntity deserializeCell(const QJsonObject& object)
{
    CellEntity entity;
    deserializeBaseEntity(object, entity, cad::battery::EntityKind::Cell);
    entity.position = vec3FromJson(object.value("position"), entity.position);
    entity.form_factor = cellFormFactorFromJson(object.value("form_factor"), entity.form_factor);
    entity.radius = static_cast<float>(object.value("radius").toDouble(entity.radius));
    entity.height = static_cast<float>(object.value("height").toDouble(entity.height));
    entity.width = static_cast<float>(object.value("width").toDouble(entity.width));
    entity.depth = static_cast<float>(object.value("depth").toDouble(entity.depth));
    entity.series_index = object.value("series_index").toInt(entity.series_index);
    entity.parallel_index = object.value("parallel_index").toInt(entity.parallel_index);
    entity.cell_type = object.value("cell_type").toString(QString::fromStdString(entity.cell_type)).toStdString();
    return entity;
}

QJsonObject serializeBusbar(const BusbarEntity& entity)
{
    QJsonObject object = serializeBaseEntity(entity);
    object.insert("module_index", entity.module_index);
    object.insert("role", busbarRoleToString(entity.role));
    object.insert("center", vec3ToJson(entity.center));
    object.insert("size", vec3ToJson(entity.size));
    return object;
}

BusbarEntity deserializeBusbar(const QJsonObject& object)
{
    BusbarEntity entity;
    deserializeBaseEntity(object, entity, cad::battery::EntityKind::Busbar);
    entity.module_index = object.value("module_index").toInt(entity.module_index);
    entity.role = busbarRoleFromJson(object.value("role"), entity.role);
    entity.center = vec3FromJson(object.value("center"), entity.center);
    entity.size = vec3FromJson(object.value("size"), entity.size);
    return entity;
}

QJsonObject serializeCoolingPlate(const CoolingPlateEntity& entity)
{
    QJsonObject object = serializeBaseEntity(entity);
    object.insert("plate_index", entity.plate_index);
    object.insert("center", vec3ToJson(entity.center));
    object.insert("size", vec3ToJson(entity.size));
    return object;
}

CoolingPlateEntity deserializeCoolingPlate(const QJsonObject& object)
{
    CoolingPlateEntity entity;
    deserializeBaseEntity(object, entity, cad::battery::EntityKind::CoolingPlate);
    entity.plate_index = object.value("plate_index").toInt(entity.plate_index);
    entity.center = vec3FromJson(object.value("center"), entity.center);
    entity.size = vec3FromJson(object.value("size"), entity.size);
    return entity;
}

QJsonObject serializeEnclosure(const PackEnclosureEntity& entity)
{
    QJsonObject object = serializeBaseEntity(entity);
    object.insert("enclosure_index", entity.enclosure_index);
    object.insert("center", vec3ToJson(entity.center));
    object.insert("size", vec3ToJson(entity.size));
    object.insert("wall_thickness", entity.wall_thickness);
    return object;
}

PackEnclosureEntity deserializeEnclosure(const QJsonObject& object)
{
    PackEnclosureEntity entity;
    deserializeBaseEntity(object, entity, cad::battery::EntityKind::PackEnclosure);
    entity.enclosure_index = object.value("enclosure_index").toInt(entity.enclosure_index);
    entity.center = vec3FromJson(object.value("center"), entity.center);
    entity.size = vec3FromJson(object.value("size"), entity.size);
    entity.wall_thickness = static_cast<float>(object.value("wall_thickness").toDouble(entity.wall_thickness));
    return entity;
}

template <typename Entity, typename Serializer>
QJsonArray serializeEntityArray(const std::vector<Entity>& entities, Serializer serializer)
{
    QJsonArray array;
    for (const auto& entity : entities) {
        array.append(serializer(entity));
    }
    return array;
}

template <typename Deserializer>
bool restoreEntityArray(
    const QJsonArray& array,
    const QString& label,
    cad::core::CadDocument& document,
    std::uint64_t& max_entity_id,
    QString* error,
    Deserializer deserialize_entity)
{
    for (qsizetype index = 0; index < array.size(); ++index) {
        if (!array.at(index).isObject()) {
            if (error != nullptr) {
                *error = QString("CAD document field '%1' must contain only objects.").arg(label);
            }
            return false;
        }

        const auto entity = deserialize_entity(array.at(index).toObject());
        max_entity_id = std::max(max_entity_id, entity.id.value);
        if (!document.restoreEntity(entity)) {
            if (error != nullptr) {
                *error = QString("Could not restore CAD %1 entry at index %2.").arg(label).arg(index);
            }
            return false;
        }
    }
    return true;
}

} // namespace

QJsonObject serializeCadDocument(const core::CadDocument& document)
{
    QJsonObject object;
    object.insert("version", kCadDocumentVersion);
    object.insert("next_entity_id", entityIdToString(EntityId{document.nextEntityIdValue()}));
    object.insert("metadata", serializeMetadata(document.metadata()));

    QJsonObject selection;
    selection.insert("primary_id", entityIdToString(document.selection().primary));
    object.insert("selection", selection);

    QJsonObject entities;
    entities.insert("packs", serializeEntityArray(document.packs(), serializePack));
    entities.insert("modules", serializeEntityArray(document.moduleBoundaries(), serializeModule));
    entities.insert("groups", serializeEntityArray(document.cellGroups(), serializeGroup));
    entities.insert("cells", serializeEntityArray(document.cells(), serializeCell));
    entities.insert("busbars", serializeEntityArray(document.busbars(), serializeBusbar));
    entities.insert("cooling_plates", serializeEntityArray(document.coolingPlates(), serializeCoolingPlate));
    entities.insert("enclosures", serializeEntityArray(document.packEnclosures(), serializeEnclosure));
    object.insert("entities", entities);

    return object;
}

bool deserializeCadDocument(const QJsonObject& object, core::CadDocument& document, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }

    const QJsonObject entities = object.value("entities").toObject();
    if (entities.isEmpty()) {
        if (error != nullptr) {
            *error = "CAD document payload is missing the 'entities' object.";
        }
        return false;
    }

    core::CadDocument loaded_document;
    loaded_document.metadata() = deserializeMetadata(object.value("metadata").toObject());

    std::uint64_t max_entity_id = 0;
    if (!restoreEntityArray(entities.value("packs").toArray(), "packs", loaded_document, max_entity_id, error, deserializePack)) {
        return false;
    }
    if (!restoreEntityArray(entities.value("modules").toArray(), "modules", loaded_document, max_entity_id, error, deserializeModule)) {
        return false;
    }
    if (!restoreEntityArray(entities.value("groups").toArray(), "groups", loaded_document, max_entity_id, error, deserializeGroup)) {
        return false;
    }
    if (!restoreEntityArray(entities.value("cells").toArray(), "cells", loaded_document, max_entity_id, error, deserializeCell)) {
        return false;
    }
    if (!restoreEntityArray(entities.value("busbars").toArray(), "busbars", loaded_document, max_entity_id, error, deserializeBusbar)) {
        return false;
    }
    if (!restoreEntityArray(entities.value("cooling_plates").toArray(), "cooling_plates", loaded_document, max_entity_id, error, deserializeCoolingPlate)) {
        return false;
    }
    if (!restoreEntityArray(entities.value("enclosures").toArray(), "enclosures", loaded_document, max_entity_id, error, deserializeEnclosure)) {
        return false;
    }

    const std::uint64_t requested_next_entity_id = entityIdFromJson(object.value("next_entity_id")).value;
    loaded_document.setNextEntityIdValue(std::max(max_entity_id + 1, requested_next_entity_id > 0 ? requested_next_entity_id : max_entity_id + 1));

    loaded_document.clearSelection();
    const EntityId selected_entity_id = entityIdFromJson(object.value("selection").toObject().value("primary_id"));
    if (selected_entity_id.isValid() && loaded_document.hasEntity(selected_entity_id)) {
        loaded_document.selectEntity(selected_entity_id);
    }

    document = std::move(loaded_document);
    return true;
}

bool tryLoadCadDocumentFromProject(const QJsonObject& project_root, core::CadDocument& document, QString* error)
{
    if (error != nullptr) {
        error->clear();
    }

    const QJsonValue cad_document = project_root.value("cad_document");
    if (cad_document.isUndefined() || cad_document.isNull()) {
        return false;
    }
    if (!cad_document.isObject()) {
        if (error != nullptr) {
            *error = "Project CAD document payload is not a JSON object.";
        }
        return false;
    }

    return deserializeCadDocument(cad_document.toObject(), document, error);
}

} // namespace cad::io
