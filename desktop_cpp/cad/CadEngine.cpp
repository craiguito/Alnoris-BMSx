#include "CadEngine.h"

#include "battery/BatteryVisualizationBuilder.h"
#include "battery/PackLayoutGenerator.h"

#include <algorithm>
#include <utility>

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
    m_document.selectEntity(entity_id);
    rebuildRenderPacket();
}

void CadEngine::clearSelection()
{
    m_document.clearSelection();
    rebuildRenderPacket();
}

bool CadEngine::moveEntity(core::EntityId entity_id, const math::Vec3& delta)
{
    const bool changed = m_document.moveEntity(entity_id, delta);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::setEntityPosition(core::EntityId entity_id, const math::Vec3& position)
{
    const bool changed = m_document.setEntityPosition(entity_id, position);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::removeEntity(core::EntityId entity_id)
{
    const bool changed = m_document.removeEntity(entity_id);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::restoreEntity(const battery::EntityRecord& entity)
{
    const bool changed = m_document.restoreEntity(entity);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::setEntityLabel(core::EntityId entity_id, std::string label)
{
    const bool changed = m_document.setEntityLabel(entity_id, std::move(label));
    if (changed) {
        refreshDocumentView(false);
    }
    return changed;
}

bool CadEngine::setCellPosition(core::EntityId entity_id, const math::Vec3& position)
{
    const bool changed = m_document.setCellPosition(entity_id, position);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::setCellGeometry(core::EntityId entity_id, float radius, float height)
{
    const bool changed = m_document.setCellGeometry(entity_id, radius, height);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::setBusbarGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size)
{
    const bool changed = m_document.setBusbarGeometry(entity_id, center, size);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::setCoolingPlateGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size)
{
    const bool changed = m_document.setCoolingPlateGeometry(entity_id, center, size);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::setModuleBoundaryGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size)
{
    const bool changed = m_document.setModuleBoundaryGeometry(entity_id, center, size);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::setEnclosureGeometry(core::EntityId entity_id, const math::Vec3& center, const math::Vec3& size, float wall_thickness)
{
    const bool changed = m_document.setEnclosureGeometry(entity_id, center, size, wall_thickness);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::executeCommand(std::unique_ptr<commands::ICommand> command)
{
    return m_commandStack.execute(std::move(command), *this);
}

bool CadEngine::undo()
{
    return m_commandStack.undo(*this);
}

bool CadEngine::redo()
{
    return m_commandStack.redo(*this);
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

std::optional<battery::EntityRecord> CadEngine::snapshotEntity(core::EntityId entity_id) const
{
    return m_document.snapshotEntity(entity_id);
}

std::optional<battery::EntitySummary> CadEngine::getEntitySummary(core::EntityId entity_id) const
{
    return m_document.getEntitySummary(entity_id);
}

std::optional<battery::EntitySummary> CadEngine::getSelectedEntitySummary() const
{
    return m_document.getSelectedEntitySummary();
}

std::optional<battery::CellProperties> CadEngine::getCellProperties(core::EntityId entity_id) const
{
    return m_document.getCellProperties(entity_id);
}

std::optional<battery::BusbarProperties> CadEngine::getBusbarProperties(core::EntityId entity_id) const
{
    return m_document.getBusbarProperties(entity_id);
}

std::optional<battery::CoolingPlateProperties> CadEngine::getCoolingPlateProperties(core::EntityId entity_id) const
{
    return m_document.getCoolingPlateProperties(entity_id);
}

std::optional<battery::ModuleBoundaryProperties> CadEngine::getModuleBoundaryProperties(core::EntityId entity_id) const
{
    return m_document.getModuleBoundaryProperties(entity_id);
}

std::optional<battery::PackEnclosureProperties> CadEngine::getEnclosureProperties(core::EntityId entity_id) const
{
    return m_document.getEnclosureProperties(entity_id);
}

void CadEngine::rebuildDocument()
{
    const std::string existing_mesh_path = m_document.metadata().cell_mesh_path;
    battery::PackLayoutGenerator::rebuildDocument(m_document, m_config.layout);
    m_document.metadata().cell_mesh_path = existing_mesh_path;
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

void CadEngine::refreshDocumentView(bool rebuild_visualization)
{
    if (rebuild_visualization) {
        rebuildVisualization();
    }
    rebuildRenderPacket();
}

} // namespace cad
