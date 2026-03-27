#pragma once

#include "battery/BatteryConfig.h"
#include "battery/BatteryEntities.h"
#include "camera/Camera.h"
#include "commands/CommandStack.h"
#include "core/CadDocument.h"
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
    CadEngine();

    bool setCellMeshPath(const std::string& path);
    void setViewportSize(int width, int height);
    void setBatteryConfig(const battery::BatteryCadConfig& config);
    void orbit(float delta_yaw_deg, float delta_pitch_deg);
    void zoom(float delta);
    void selectEntity(core::EntityId entity_id);
    void clearSelection();
    bool moveEntity(core::EntityId entity_id, const math::Vec3& delta);
    bool setEntityPosition(core::EntityId entity_id, const math::Vec3& position);
    bool removeEntity(core::EntityId entity_id);
    bool restoreEntity(const battery::EntityRecord& entity);
    bool setEntityLabel(core::EntityId entity_id, std::string label);
    bool setCellPosition(core::EntityId entity_id, const math::Vec3& position);
    bool setCellGeometry(core::EntityId entity_id, float radius, float height);
    bool setBusbarGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size);
    bool setCoolingPlateGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size);
    bool setModuleBoundaryGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size);
    bool setEnclosureGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size, float wall_thickness);

    bool executeCommand(std::unique_ptr<commands::ICommand> command);
    bool undo();
    bool redo();

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

private:
    void rebuildDocument();
    void rebuildVisualization();
    void rebuildRenderPacket();
    void refreshDocumentView(bool rebuild_visualization);

    battery::BatteryCadConfig m_config;
    core::CadDocument m_document;
    battery::BatteryVisualizationOverlay m_visualizationOverlay;
    camera::Camera m_camera;
    render::RenderComposer m_renderComposer;
    picking::HitTester m_hitTester;
    io::TriangleMesh m_cellMesh;
    int m_viewportWidth = 1;
    int m_viewportHeight = 1;
    render::RenderPacket m_renderPacket;
    commands::CommandStack m_commandStack;
};

} // namespace cad
