#pragma once

#include "battery/BatteryConfig.h"
#include "battery/BatteryEntities.h"
#include "camera/Camera.h"
#include "commands/CommandStack.h"
#include "core/CadDocument.h"
#include "geometry/BatteryGeometryGenerator.h"
#include "io/MeshLoader.h"
#include "picking/HitTester.h"
#include "render/RenderComposer.h"

#include <memory>
#include <optional>
#include <string>

namespace cad {

class CadEngine
{
public:
    struct RenderDiagnostics
    {
        double last_geometry_build_ms = 0.0;
        double last_render_packet_ms = 0.0;
        std::size_t geometry_rebuild_count = 0;
        std::size_t packet_rebuild_count = 0;
        std::size_t triangle_count = 0;
        std::size_t line_count = 0;
    };

    CadEngine();

    bool setCellMeshPath(const std::string& path);
    void setViewportSize(int width, int height);
    void setBatteryConfig(const battery::BatteryCadConfig& config);
    void setVisualizationOverlay(const battery::BatteryVisualizationOverlay& overlay);
    void clearVisualizationOverlay();
    void orbit(float delta_yaw_deg, float delta_pitch_deg);
    void zoom(float delta);
    void selectEntity(core::EntityId entity_id);
    void clearSelection();
    bool moveEntity(core::EntityId entity_id, const math::Vec3& delta);
    bool setEntityPosition(core::EntityId entity_id, const math::Vec3& position);
    bool removeEntity(core::EntityId entity_id);
    bool restoreEntity(const battery::EntityRecord& entity);
    bool setEntityLabel(core::EntityId entity_id, std::string label);
    bool setEntityVisibility(core::EntityId entity_id, bool visible);
    bool setCellPosition(core::EntityId entity_id, const math::Vec3& position);
    bool setCellGeometry(core::EntityId entity_id, float radius, float height);
    bool setBusbarGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size);
    bool setCoolingPlateGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size);
    bool setModuleBoundaryGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size);
    bool setEnclosureGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size, float wall_thickness);
    bool updateCellProperties(core::EntityId entity_id, const battery::CellPropertiesUpdate& update);
    bool updateBusbarProperties(core::EntityId entity_id, const battery::BusbarPropertiesUpdate& update);
    bool updateCoolingPlateProperties(core::EntityId entity_id, const battery::CoolingPlatePropertiesUpdate& update);
    bool updateModuleBoundaryProperties(core::EntityId entity_id, const battery::ModuleBoundaryPropertiesUpdate& update);
    bool updateEnclosureProperties(core::EntityId entity_id, const battery::PackEnclosurePropertiesUpdate& update);
    bool applyCellProperties(core::EntityId entity_id, const battery::CellProperties& properties);
    bool applyBusbarProperties(core::EntityId entity_id, const battery::BusbarProperties& properties);
    bool applyCoolingPlateProperties(core::EntityId entity_id, const battery::CoolingPlateProperties& properties);
    bool applyModuleBoundaryProperties(core::EntityId entity_id, const battery::ModuleBoundaryProperties& properties);
    bool applyEnclosureProperties(core::EntityId entity_id, const battery::PackEnclosureProperties& properties);
    bool resetEntityPositionToGenerated(core::EntityId entity_id);
    bool resetEntityGeometryToGenerated(core::EntityId entity_id);
    bool resetEntityLabelToGenerated(core::EntityId entity_id);

    bool executeCommand(std::unique_ptr<commands::ICommand> command);
    bool undo();
    bool redo();
    bool applyMoveEntity(core::EntityId entity_id, const math::Vec3& delta);
    bool applyRemoveEntity(core::EntityId entity_id);
    bool applyRenameEntity(core::EntityId entity_id, std::string label);
    bool applySetEntityVisibility(core::EntityId entity_id, bool visible);
    bool applyCellPropertiesUpdate(core::EntityId entity_id, const battery::CellPropertiesUpdate& update);
    bool applyBusbarPropertiesUpdate(core::EntityId entity_id, const battery::BusbarPropertiesUpdate& update);
    bool applyCoolingPlatePropertiesUpdate(core::EntityId entity_id, const battery::CoolingPlatePropertiesUpdate& update);
    bool applyModuleBoundaryPropertiesUpdate(core::EntityId entity_id, const battery::ModuleBoundaryPropertiesUpdate& update);
    bool applyEnclosurePropertiesUpdate(core::EntityId entity_id, const battery::PackEnclosurePropertiesUpdate& update);
    bool applyResetEntityPositionToGenerated(core::EntityId entity_id);
    bool applyResetEntityGeometryToGenerated(core::EntityId entity_id);
    bool applyResetEntityLabelToGenerated(core::EntityId entity_id);

    [[nodiscard]] core::EntityId hitTestEntity(float x, float y) const;

    [[nodiscard]] const render::RenderPacket& renderPacket() const;
    [[nodiscard]] core::EntityId selectedEntity() const;
    [[nodiscard]] const core::CadDocument& document() const;
    [[nodiscard]] std::optional<battery::EntityRecord> snapshotEntity(core::EntityId entity_id) const;
    [[nodiscard]] std::optional<battery::EntitySummary> getEntitySummary(core::EntityId entity_id) const;
    [[nodiscard]] std::optional<battery::EntitySummary> getSelectedEntitySummary() const;
    [[nodiscard]] std::optional<battery::CellProperties> getCellProperties(core::EntityId entity_id) const;
    [[nodiscard]] std::optional<battery::BusbarProperties> getBusbarProperties(core::EntityId entity_id) const;
    [[nodiscard]] std::optional<battery::CoolingPlateProperties> getCoolingPlateProperties(core::EntityId entity_id) const;
    [[nodiscard]] std::optional<battery::ModuleBoundaryProperties> getModuleBoundaryProperties(core::EntityId entity_id) const;
    [[nodiscard]] std::optional<battery::PackEnclosureProperties> getEnclosureProperties(core::EntityId entity_id) const;
    [[nodiscard]] const RenderDiagnostics& renderDiagnostics() const { return m_renderDiagnostics; }

private:
    void rebuildDocument();
    void rebuildVisualization();
    void rebuildVisualGeometry();
    void rebuildRenderPacket();
    void refreshDocumentView(bool rebuild_visualization);

    battery::BatteryCadConfig m_config;
    core::CadDocument m_document;
    battery::BatteryVisualizationOverlay m_visualizationOverlay;
    std::optional<battery::BatteryVisualizationOverlay> m_overrideVisualizationOverlay;
    camera::Camera m_camera;
    geometry::BatteryGeometryGenerator m_geometryGenerator;
    geometry::GeometryBuffer m_visualGeometry;
    render::RenderComposer m_renderComposer;
    picking::HitTester m_hitTester;
    io::TriangleMesh m_cellMesh;
    int m_viewportWidth = 1;
    int m_viewportHeight = 1;
    render::RenderPacket m_renderPacket;
    RenderDiagnostics m_renderDiagnostics;
    commands::CommandStack m_commandStack;
};

} // namespace cad
