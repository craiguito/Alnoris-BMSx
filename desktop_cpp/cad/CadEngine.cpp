#include "CadEngine.h"

#include "battery/BatteryVisualizationBuilder.h"
#include "battery/PackLayoutGenerator.h"

#include <algorithm>

namespace cad {

CadEngine::CadEngine()
{
    rebuildDocument();
    rebuildVisualization();
    rebuildRenderPacket();
}

bool CadEngine::setCellMeshPath(const std::string& path)
{
    io::TriangleMesh mesh;
    if (!io::MeshLoader::loadBinaryStl(path, mesh)) {
        return false;
    }

    m_cellMesh = std::move(mesh);
    m_document.metadata().cell_mesh_path = path;
    rebuildRenderPacket();
    return true;
}

void CadEngine::setViewportSize(int width, int height)
{
    m_viewportWidth = std::max(1, width);
    m_viewportHeight = std::max(1, height);
    rebuildRenderPacket();
}

void CadEngine::setBatteryConfig(const battery::BatteryCadConfig& config)
{
    m_config = config;
    rebuildDocument();
    rebuildVisualization();
    rebuildRenderPacket();
}

void CadEngine::orbit(float delta_yaw_deg, float delta_pitch_deg)
{
    m_camera.orbit(delta_yaw_deg, delta_pitch_deg);
    rebuildRenderPacket();
}

void CadEngine::zoom(float delta)
{
    m_camera.zoom(delta);
    rebuildRenderPacket();
}

void CadEngine::selectEntity(core::EntityId entity_id)
{
    if (!m_document.hasEntity(entity_id)) {
        m_document.selection().clear();
    } else {
        m_document.selection().primary = entity_id;
    }
    rebuildRenderPacket();
}

core::EntityId CadEngine::hitTestEntity(float x, float y) const
{
    return m_hitTester.hitTestEntity(m_renderPacket, x, y);
}

const render::RenderPacket& CadEngine::renderPacket() const
{
    return m_renderPacket;
}

core::EntityId CadEngine::selectedEntity() const
{
    return m_document.selection().primary;
}

const core::CadDocument& CadEngine::document() const
{
    return m_document;
}

void CadEngine::rebuildDocument()
{
    const std::string existing_mesh_path = m_document.metadata().cell_mesh_path;
    battery::PackLayoutGenerator::rebuildDocument(m_document, m_config.layout);
    m_document.metadata().cell_mesh_path = existing_mesh_path;
    if (!m_document.cells().empty()) {
        m_document.selection().primary = m_document.cells().front().id;
    }
}

void CadEngine::rebuildVisualization()
{
    m_visualizationOverlay = battery::BatteryVisualizationBuilder::build(
        m_document,
        m_config.electrical,
        m_config.thermal
    );
}

void CadEngine::rebuildRenderPacket()
{
    m_renderPacket = m_renderComposer.compose(
        m_document,
        m_visualizationOverlay,
        m_camera,
        m_cellMesh.vertices.empty() ? nullptr : &m_cellMesh,
        m_viewportWidth,
        m_viewportHeight
    );
}

} // namespace cad
