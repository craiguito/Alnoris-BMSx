#include "CadEngine.h"

#include "battery/BatteryVisualizationBuilder.h"
#include "battery/PackLayoutGenerator.h"
#include "commands/BatteryCommands.h"
#include "edit/CadEditService.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace cad {
namespace {

bool isNearlyZeroDelta(const math::Vec3& delta)
{
    return std::fabs(delta.x) <= 0.0001f
        && std::fabs(delta.y) <= 0.0001f
        && std::fabs(delta.z) <= 0.0001f;
}

} // namespace

CadEngine::CadEngine()
{
    rebuildDocument();
    rebuildVisualization();
    rebuildVisualGeometry();
    fitCameraToVisualGeometry();
    rebuildRenderPacket();
}

bool CadEngine::setCellMeshPath(const std::string& path)
{
    cancelInteractiveMove();

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
    cancelInteractiveMove();

    m_config = config;
    m_commandStack.clear();
    m_overrideVisualizationOverlay.reset();
    rebuildDocument();
    rebuildVisualization();
    rebuildVisualGeometry();
    fitCameraToVisualGeometry();
    rebuildRenderPacket();
}

void CadEngine::loadDocument(core::CadDocument document, const battery::BatteryCadConfig& config)
{
    cancelInteractiveMove();

    m_config = config;
    if (document.metadata().layout_config.cells_in_series > 0 || document.metadata().layout_config.cells_in_parallel > 0) {
        m_config.layout = document.metadata().layout_config;
    }

    m_document = std::move(document);
    if (m_document.selection().primary.isValid() && !m_document.hasEntity(m_document.selection().primary)) {
        m_document.clearSelection();
    }

    m_commandStack.clear();
    m_overrideVisualizationOverlay.reset();
    m_cellMesh.vertices.clear();
    if (!m_document.metadata().cell_mesh_path.empty()) {
        io::TriangleMesh mesh;
        if (io::MeshLoader::loadBinaryStl(m_document.metadata().cell_mesh_path, mesh)) {
            m_cellMesh = std::move(mesh);
        }
    }

    rebuildVisualization();
    rebuildVisualGeometry();
    fitCameraToVisualGeometry();
    rebuildRenderPacket();
}

void CadEngine::setVisualizationOverlay(const battery::BatteryVisualizationOverlay& overlay)
{
    cancelInteractiveMove();

    m_overrideVisualizationOverlay = overlay;
    m_visualizationOverlay = overlay;
    rebuildVisualGeometry();
    rebuildRenderPacket();
}

void CadEngine::clearVisualizationOverlay()
{
    cancelInteractiveMove();

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
    if (m_interactiveMove.has_value() && m_interactiveMove->entity_id != entity_id) {
        cancelInteractiveMove();
    }
    m_document.selectEntity(entity_id);
    rebuildRenderPacket();
}

void CadEngine::clearSelection()
{
    cancelInteractiveMove();
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

bool CadEngine::restoreEntities(const std::vector<battery::EntityRecord>& entities)
{
    bool changed = false;
    for (const auto& entity : entities) {
        changed = m_document.restoreEntity(entity) || changed;
    }
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
    const bool changed = edit::CadEditService::resetEntityLabelToGenerated(m_document, m_config.layout, entity_id);
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
    cancelInteractiveMove();
    return m_commandStack.undo(*this);
}

bool CadEngine::redo()
{
    cancelInteractiveMove();
    return m_commandStack.redo(*this);
}

bool CadEngine::beginInteractiveMove(core::EntityId entity_id)
{
    if (!entity_id.isValid()) {
        return false;
    }

    if (m_interactiveMove.has_value()) {
        if (m_interactiveMove->entity_id == entity_id) {
            return true;
        }
        cancelInteractiveMove();
    }

    const auto snapshot = snapshotEntity(entity_id);
    if (!snapshot.has_value()) {
        return false;
    }

    m_interactiveMove = InteractiveMoveState{entity_id, *snapshot, {}};
    return true;
}

bool CadEngine::updateInteractiveMovePreview(const math::Vec3& delta)
{
    if (!m_interactiveMove.has_value()) {
        return false;
    }

    if (isNearlyZeroDelta(delta)) {
        return true;
    }

    if (!previewMoveEntity(m_interactiveMove->entity_id, delta)) {
        return false;
    }

    m_interactiveMove->accumulated_delta = math::add(m_interactiveMove->accumulated_delta, delta);
    return true;
}

bool CadEngine::commitInteractiveMove()
{
    if (!m_interactiveMove.has_value()) {
        return false;
    }

    const InteractiveMoveState transaction = *m_interactiveMove;
    m_interactiveMove.reset();

    if (isNearlyZeroDelta(transaction.accumulated_delta)) {
        restoreInteractiveMoveState(transaction.original_state);
        return false;
    }

    if (!restoreInteractiveMoveState(transaction.original_state)) {
        return false;
    }

    return executeCommand(std::make_unique<commands::MoveEntityCommand>(
        transaction.entity_id,
        transaction.accumulated_delta));
}

bool CadEngine::cancelInteractiveMove()
{
    if (!m_interactiveMove.has_value()) {
        return false;
    }

    const battery::EntityRecord original_state = m_interactiveMove->original_state;
    m_interactiveMove.reset();
    return restoreInteractiveMoveState(original_state);
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

const geometry::GeometryBuffer& CadEngine::visualGeometry() const
{
    return m_visualGeometry;
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

bool CadEngine::canUndo() const
{
    return m_commandStack.canUndo();
}

bool CadEngine::canRedo() const
{
    return m_commandStack.canRedo();
}

bool CadEngine::hasInteractiveMove() const
{
    return m_interactiveMove.has_value();
}

void CadEngine::rebuildDocument()
{
    const std::string existing_mesh_path = m_document.metadata().cell_mesh_path;
    battery::PackLayoutGenerator::rebuildDocument(m_document, m_config.layout);
    m_document.metadata().cell_mesh_path = existing_mesh_path;
    const battery::BoundingBox bounds = sceneBounds();
    m_camera.fitToBounds(bounds.center, bounds.size);
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
    m_renderDiagnostics.line_count =
        m_visualGeometry.lines.size()
        + (m_renderPacket.lines.size() / 2)
        + (m_renderPacket.selection_overlay_lines.size() / 2);
}

void CadEngine::refreshDocumentView(bool rebuild_visualization)
{
    if (rebuild_visualization) {
        rebuildVisualization();
    }
    rebuildVisualGeometry();
    rebuildRenderPacket();
}

bool CadEngine::restoreInteractiveMoveState(const battery::EntityRecord& entity)
{
    const bool changed = m_document.restoreEntity(entity);
    if (changed) {
        refreshDocumentView(false);
    }
    return changed;
}

bool CadEngine::previewMoveEntity(core::EntityId entity_id, const math::Vec3& delta)
{
    const bool changed = edit::CadEditService::moveEntity(m_document, entity_id, delta);
    if (changed) {
        refreshDocumentView(false);
    }
    return changed;
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
    m_renderDiagnostics.triangle_count = m_visualGeometry.triangles.size();
    m_renderDiagnostics.line_count = m_visualGeometry.lines.size();
    const auto& cache_stats = m_geometryGenerator.cacheStats();
    m_renderDiagnostics.cylindrical_cell_cache_hits = cache_stats.cylindrical_cell_hits;
    m_renderDiagnostics.cylindrical_cell_cache_misses = cache_stats.cylindrical_cell_misses;
}

battery::BoundingBox CadEngine::sceneBounds() const
{
    if (!m_document.packEnclosures().empty()) {
        return m_document.worldBounds(m_document.packEnclosures().front().id);
    }
    if (!m_document.packs().empty()) {
        return m_document.worldBounds(m_document.packs().front().id);
    }
    return {{0.0f, 0.0f, 0.0f}, {320.0f, 220.0f, 240.0f}};
}

battery::BoundingBox CadEngine::visualGeometryBounds() const
{
    math::Vec3 min_point{};
    math::Vec3 max_point{};
    bool initialized = false;

    for (const auto& triangle : m_visualGeometry.triangles) {
        const math::Vec3 vertices[] = {triangle.a, triangle.b, triangle.c};
        for (const math::Vec3& vertex : vertices) {
            if (!initialized) {
                min_point = vertex;
                max_point = vertex;
                initialized = true;
                continue;
            }
            min_point.x = std::min(min_point.x, vertex.x);
            min_point.y = std::min(min_point.y, vertex.y);
            min_point.z = std::min(min_point.z, vertex.z);
            max_point.x = std::max(max_point.x, vertex.x);
            max_point.y = std::max(max_point.y, vertex.y);
            max_point.z = std::max(max_point.z, vertex.z);
        }
    }

    for (const auto& line : m_visualGeometry.lines) {
        const math::Vec3 vertices[] = {line.a, line.b};
        for (const math::Vec3& vertex : vertices) {
            if (!initialized) {
                min_point = vertex;
                max_point = vertex;
                initialized = true;
                continue;
            }
            min_point.x = std::min(min_point.x, vertex.x);
            min_point.y = std::min(min_point.y, vertex.y);
            min_point.z = std::min(min_point.z, vertex.z);
            max_point.x = std::max(max_point.x, vertex.x);
            max_point.y = std::max(max_point.y, vertex.y);
            max_point.z = std::max(max_point.z, vertex.z);
        }
    }

    if (!initialized) {
        return sceneBounds();
    }

    return {
        {
            (min_point.x + max_point.x) * 0.5f,
            (min_point.y + max_point.y) * 0.5f,
            (min_point.z + max_point.z) * 0.5f
        },
        {
            std::max(1.0f, max_point.x - min_point.x),
            std::max(1.0f, max_point.y - min_point.y),
            std::max(1.0f, max_point.z - min_point.z)
        }
    };
}

void CadEngine::fitCameraToVisualGeometry()
{
    const battery::BoundingBox bounds = visualGeometryBounds();
    m_camera.fitToBounds(bounds.center, bounds.size);
}

} // namespace cad
