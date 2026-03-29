#include "CadEngine.h"

#include "battery/BatteryVisualizationBuilder.h"
#include "battery/PackLayoutGenerator.h"
#include "commands/BatteryCommands.h"
#include "edit/CadEditService.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace cad {

CadEngine::CadEngine()
{
    rebuildDocument();
    rebuildVisualization();
    rebuildVisualGeometry();
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
    rebuildVisualGeometry();
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
    m_overrideVisualizationOverlay.reset();
    rebuildDocument();
    rebuildVisualization();
    rebuildVisualGeometry();
    rebuildRenderPacket();
}

void CadEngine::setVisualizationOverlay(const battery::BatteryVisualizationOverlay& overlay)
{
    m_overrideVisualizationOverlay = overlay;
    m_visualizationOverlay = overlay;
    rebuildVisualGeometry();
    rebuildRenderPacket();
}

void CadEngine::clearVisualizationOverlay()
{
    m_overrideVisualizationOverlay.reset();
    rebuildVisualization();
    rebuildVisualGeometry();
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
    const bool changed = edit::CadEditService::moveEntity(m_document, entity_id, delta);
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
    const bool changed = edit::CadEditService::setEntityLabel(m_document, entity_id, std::move(label));
    if (changed) {
        refreshDocumentView(false);
    }
    return changed;
}

bool CadEngine::setEntityVisibility(core::EntityId entity_id, bool visible)
{
    const bool changed = edit::CadEditService::setEntityVisibility(m_document, entity_id, visible);
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

bool CadEngine::updateCellProperties(core::EntityId entity_id, const battery::CellPropertiesUpdate& update)
{
    const bool changed = edit::CadEditService::updateCellProperties(m_document, entity_id, update);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::updateBusbarProperties(core::EntityId entity_id, const battery::BusbarPropertiesUpdate& update)
{
    const bool changed = edit::CadEditService::updateBusbarProperties(m_document, entity_id, update);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::updateCoolingPlateProperties(core::EntityId entity_id, const battery::CoolingPlatePropertiesUpdate& update)
{
    const bool changed = edit::CadEditService::updateCoolingPlateProperties(m_document, entity_id, update);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::updateModuleBoundaryProperties(core::EntityId entity_id, const battery::ModuleBoundaryPropertiesUpdate& update)
{
    const bool changed = edit::CadEditService::updateModuleBoundaryProperties(m_document, entity_id, update);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::updateEnclosureProperties(core::EntityId entity_id, const battery::PackEnclosurePropertiesUpdate& update)
{
    const bool changed = edit::CadEditService::updateEnclosureProperties(m_document, entity_id, update);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::applyCellProperties(core::EntityId entity_id, const battery::CellProperties& properties)
{
    const bool changed = edit::CadEditService::applyCellProperties(m_document, entity_id, properties);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::applyBusbarProperties(core::EntityId entity_id, const battery::BusbarProperties& properties)
{
    const bool changed = edit::CadEditService::applyBusbarProperties(m_document, entity_id, properties);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::applyCoolingPlateProperties(core::EntityId entity_id, const battery::CoolingPlateProperties& properties)
{
    const bool changed = edit::CadEditService::applyCoolingPlateProperties(m_document, entity_id, properties);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::applyModuleBoundaryProperties(core::EntityId entity_id, const battery::ModuleBoundaryProperties& properties)
{
    const bool changed = edit::CadEditService::applyModuleBoundaryProperties(m_document, entity_id, properties);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::applyEnclosureProperties(core::EntityId entity_id, const battery::PackEnclosureProperties& properties)
{
    const bool changed = edit::CadEditService::applyEnclosureProperties(m_document, entity_id, properties);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::resetEntityPositionToGenerated(core::EntityId entity_id)
{
    const bool changed = edit::CadEditService::resetEntityPositionToGenerated(m_document, m_config.layout, entity_id);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::resetEntityGeometryToGenerated(core::EntityId entity_id)
{
    const bool changed = edit::CadEditService::resetEntityGeometryToGenerated(m_document, m_config.layout, entity_id);
    if (changed) {
        refreshDocumentView(true);
    }
    return changed;
}

bool CadEngine::resetEntityLabelToGenerated(core::EntityId entity_id)
{
    const bool changed = edit::CadEditService::resetEntityLabelToGenerated(m_document, entity_id);
    if (changed) {
        refreshDocumentView(false);
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

bool CadEngine::applyMoveEntity(core::EntityId entity_id, const math::Vec3& delta)
{
    return executeCommand(std::make_unique<commands::MoveEntityCommand>(entity_id, delta));
}

bool CadEngine::applyRemoveEntity(core::EntityId entity_id)
{
    return executeCommand(std::make_unique<commands::RemoveEntityCommand>(entity_id));
}

bool CadEngine::applyRenameEntity(core::EntityId entity_id, std::string label)
{
    return executeCommand(std::make_unique<commands::RenameEntityCommand>(entity_id, std::move(label)));
}

bool CadEngine::applySetEntityVisibility(core::EntityId entity_id, bool visible)
{
    return executeCommand(std::make_unique<commands::SetEntityVisibilityCommand>(entity_id, visible));
}

bool CadEngine::applyCellPropertiesUpdate(core::EntityId entity_id, const battery::CellPropertiesUpdate& update)
{
    return executeCommand(std::make_unique<commands::UpdateCellPropertiesCommand>(entity_id, update));
}

bool CadEngine::applyBusbarPropertiesUpdate(core::EntityId entity_id, const battery::BusbarPropertiesUpdate& update)
{
    return executeCommand(std::make_unique<commands::UpdateBusbarPropertiesCommand>(entity_id, update));
}

bool CadEngine::applyCoolingPlatePropertiesUpdate(core::EntityId entity_id, const battery::CoolingPlatePropertiesUpdate& update)
{
    return executeCommand(std::make_unique<commands::UpdateCoolingPlatePropertiesCommand>(entity_id, update));
}

bool CadEngine::applyModuleBoundaryPropertiesUpdate(core::EntityId entity_id, const battery::ModuleBoundaryPropertiesUpdate& update)
{
    return executeCommand(std::make_unique<commands::UpdateModuleBoundaryPropertiesCommand>(entity_id, update));
}

bool CadEngine::applyEnclosurePropertiesUpdate(core::EntityId entity_id, const battery::PackEnclosurePropertiesUpdate& update)
{
    return executeCommand(std::make_unique<commands::UpdateEnclosurePropertiesCommand>(entity_id, update));
}

bool CadEngine::applyResetEntityPositionToGenerated(core::EntityId entity_id)
{
    return executeCommand(std::make_unique<commands::ResetEntityPositionToGeneratedCommand>(entity_id));
}

bool CadEngine::applyResetEntityGeometryToGenerated(core::EntityId entity_id)
{
    return executeCommand(std::make_unique<commands::ResetEntityGeometryToGeneratedCommand>(entity_id));
}

bool CadEngine::applyResetEntityLabelToGenerated(core::EntityId entity_id)
{
    return executeCommand(std::make_unique<commands::ResetEntityLabelToGeneratedCommand>(entity_id));
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
    if (m_overrideVisualizationOverlay.has_value()) {
        m_visualizationOverlay = *m_overrideVisualizationOverlay;
        return;
    }

    m_visualizationOverlay = battery::BatteryVisualizationBuilder::build(
        m_document,
        m_config.electrical,
        m_config.thermal
    );
}

void CadEngine::rebuildRenderPacket()
{
    const auto start = std::chrono::steady_clock::now();
    m_renderPacket = m_renderComposer.compose(
        m_document,
        m_visualizationOverlay,
        m_visualGeometry,
        m_camera,
        m_cellMesh.vertices.empty() ? nullptr : &m_cellMesh,
        m_viewportWidth,
        m_viewportHeight
    );
    const auto end = std::chrono::steady_clock::now();
    m_renderDiagnostics.last_render_packet_ms = std::chrono::duration<double, std::milli>(end - start).count();
    m_renderDiagnostics.packet_rebuild_count += 1;
    m_renderDiagnostics.triangle_count = m_renderPacket.triangles.size() / 3;
    m_renderDiagnostics.line_count = m_renderPacket.lines.size() / 2;
}

void CadEngine::refreshDocumentView(bool rebuild_visualization)
{
    if (rebuild_visualization) {
        rebuildVisualization();
    }
    rebuildVisualGeometry();
    rebuildRenderPacket();
}

void CadEngine::rebuildVisualGeometry()
{
    const auto start = std::chrono::steady_clock::now();
    m_visualGeometry = m_geometryGenerator.buildVisualGeometry(
        m_document,
        m_visualizationOverlay,
        m_cellMesh.vertices.empty() ? nullptr : &m_cellMesh
    );
    const auto end = std::chrono::steady_clock::now();
    m_renderDiagnostics.last_geometry_build_ms = std::chrono::duration<double, std::milli>(end - start).count();
    m_renderDiagnostics.geometry_rebuild_count += 1;
}

} // namespace cad
