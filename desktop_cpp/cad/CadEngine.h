#pragma once

#include "battery/BatteryConfig.h"
#include "battery/BatteryEntities.h"
#include "camera/Camera.h"
#include "core/CadDocument.h"
#include "io/MeshLoader.h"
#include "picking/HitTester.h"
#include "render/RenderComposer.h"

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
    [[nodiscard]] core::EntityId hitTestEntity(float x, float y) const;

    [[nodiscard]] const render::RenderPacket& renderPacket() const;
    [[nodiscard]] core::EntityId selectedEntity() const;
    [[nodiscard]] const core::CadDocument& document() const;

private:
    void rebuildDocument();
    void rebuildVisualization();
    void rebuildRenderPacket();

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
};

} // namespace cad
