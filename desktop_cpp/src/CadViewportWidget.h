#pragma once

#include "../cad/CadEngine.h"

#include <QColor>
#include <QOpenGLBuffer>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QPointF>
#include <QSurfaceFormat>

#include <memory>
#include <optional>
#include <vector>

class QKeyEvent;
class QMouseEvent;
class QWheelEvent;

class CadViewportWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit CadViewportWidget(QWidget* parent = nullptr);
    ~CadViewportWidget() override;

    bool setCellMeshPath(const QString& path);
    void setBackgroundColor(const QColor& color);
    void setPackConfig(const cad::battery::BatteryCadConfig& config);
    void loadDocument(cad::core::CadDocument document, const cad::battery::BatteryCadConfig& config);
    void setSimulationOverlay(const cad::battery::BatteryVisualizationOverlay& overlay);
    void clearSimulationOverlay();
    [[nodiscard]] const cad::core::CadDocument& document() const;
    [[nodiscard]] std::optional<cad::battery::EntitySummary> selectedEntitySummary() const;
    [[nodiscard]] std::optional<cad::battery::CellProperties> selectedCellProperties() const;
    [[nodiscard]] std::optional<cad::battery::BusbarProperties> selectedBusbarProperties() const;
    [[nodiscard]] std::optional<cad::battery::CoolingPlateProperties> selectedCoolingPlateProperties() const;
    [[nodiscard]] std::optional<cad::battery::ModuleBoundaryProperties> selectedModuleBoundaryProperties() const;
    [[nodiscard]] std::optional<cad::battery::PackEnclosureProperties> selectedEnclosureProperties() const;
    bool applyRenameToSelected(const QString& label);
    bool applySelectedVisibility(bool visible);
    bool applySelectedCellUpdate(const cad::battery::CellPropertiesUpdate& update);
    bool applySelectedBusbarUpdate(const cad::battery::BusbarPropertiesUpdate& update);
    bool applySelectedCoolingPlateUpdate(const cad::battery::CoolingPlatePropertiesUpdate& update);
    bool applySelectedModuleBoundaryUpdate(const cad::battery::ModuleBoundaryPropertiesUpdate& update);
    bool applySelectedEnclosureUpdate(const cad::battery::PackEnclosurePropertiesUpdate& update);
    bool undoLastEdit();
    bool redoLastEdit();
    bool resetSelectedPositionToGenerated();
    bool resetSelectedGeometryToGenerated();
    bool resetSelectedLabelToGenerated();

signals:
    void selectionChanged();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct CachedScreenTriangle
    {
        QPointF a;
        QPointF b;
        QPointF c;
        QColor color;
        float depth = 0.0f;
        unsigned char layer = 0;
    };

    struct CachedScreenLine
    {
        QPointF a;
        QPointF b;
        QColor color;
        float width = 1.0f;
    };

    enum class MoveAxis
    {
        FreeXZ,
        X,
        Y,
        Z
    };

    cad::CadEngine m_engine;
    QColor m_backgroundColor;
    std::unique_ptr<QOpenGLShaderProgram> m_triangleProgram;
    std::unique_ptr<QOpenGLShaderProgram> m_lineProgram;
    std::unique_ptr<QOpenGLFramebufferObject> m_sceneFramebuffer;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_opaqueTriangleBuffer;
    QOpenGLBuffer m_translucentTriangleBuffer;
    QOpenGLBuffer m_sceneLineBuffer;
    QOpenGLBuffer m_overlayLineBuffer;
    QOpenGLBuffer m_selectionLineBuffer;
    std::size_t m_uploadedGeometryRevision = 0;
    std::size_t m_uploadedPacketRevision = 0;
    int m_opaqueTriangleVertexCount = 0;
    int m_translucentTriangleVertexCount = 0;
    int m_sceneLineVertexCount = 0;
    int m_overlayLineVertexCount = 0;
    int m_selectionLineVertexCount = 0;
    int m_depthBits = 0;
    bool m_sceneDepthAvailable = false;
    std::vector<CachedScreenTriangle> m_cachedTriangles;
    std::vector<CachedScreenLine> m_cachedLines;
    std::vector<CachedScreenLine> m_cachedSelectionLines;
    std::size_t m_cachedSoftwareGeometryRevision = 0;
    std::size_t m_cachedSoftwarePacketRevision = 0;
    int m_cachedProjectionWidth = 0;
    int m_cachedProjectionHeight = 0;
    double m_lastProjectionBuildMs = 0.0;
    double m_lastBufferUploadMs = 0.0;
    double m_lastGpuDrawMs = 0.0;
    QPoint m_pressMousePos;
    QPoint m_lastMousePos;
    bool m_dragging = false;
    bool m_moveDragging = false;
    bool m_gridSnapEnabled = true;
    float m_gridSnapStep = 14.0f;
    MoveAxis m_moveAxis = MoveAxis::FreeXZ;

    void destroyGlResources();
    void ensureGpuResources();
    void ensureSceneFramebuffer();
    void rebuildSoftwareFallbackCache();
    void syncGpuBuffers();
    void uploadOpaqueSceneGeometry();
    void uploadTranslucentSceneGeometry();
    void uploadDynamicLines();
};
