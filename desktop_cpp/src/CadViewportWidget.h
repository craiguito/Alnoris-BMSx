#pragma once

#include "../cad/CadEngine.h"

#include <QColor>
#include <QPoint>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

class CadViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CadViewportWidget(QWidget* parent = nullptr);

    bool setCellMeshPath(const QString& path);
    void setBackgroundColor(const QColor& color);
    void setPackConfig(const cad::battery::BatteryCadConfig& config);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    cad::CadEngine m_engine;
    QColor m_backgroundColor;
    QPoint m_pressMousePos;
    QPoint m_lastMousePos;
    bool m_dragging = false;
};
