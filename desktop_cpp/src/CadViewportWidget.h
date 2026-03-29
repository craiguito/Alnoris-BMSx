#pragma once

#include "../cad/CadEngine.h"

#include <QColor>
#include <QPoint>
#include <QWidget>

#include <optional>
#include <vector>

class QMouseEvent;
class QPaintEvent;
class QKeyEvent;
class QWheelEvent;

class CadViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CadViewportWidget(QWidget* parent = nullptr);

    bool setCellMeshPath(const QString& path);
    void setBackgroundColor(const QColor& color);
    void setPackConfig(const cad::battery::BatteryCadConfig& config);
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
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
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
    std::vector<CachedScreenTriangle> m_cachedTriangles;
    std::vector<CachedScreenLine> m_cachedLines;
    std::vector<CachedScreenLine> m_cachedSelectionLines;
    std::size_t m_cachedPacketBuildCount = 0;
    int m_cachedProjectionWidth = 0;
    int m_cachedProjectionHeight = 0;
    double m_lastProjectionBuildMs = 0.0;
    QPoint m_pressMousePos;
    QPoint m_lastMousePos;
    bool m_dragging = false;
    bool m_moveDragging = false;
    bool m_gridSnapEnabled = true;
    float m_gridSnapStep = 14.0f;
    MoveAxis m_moveAxis = MoveAxis::FreeXZ;

    void rebuildScreenSpaceCache();
};
