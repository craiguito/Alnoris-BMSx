#pragma once

#include "../charts/ChartModule.h"

#include <QColor>
#include <QWidget>

#include <optional>

class QMouseEvent;
class QPaintEvent;

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* parent = nullptr);

    void setThemeColors(const QColor& background, const QColor& text);
    void showSingleSeries(const QString& title, const QString& yAxisTitle, const charts::Series& series);
    void showComparison(const QString& title, const QString& yAxisTitle, const charts::Series& baseline, const charts::Series& candidate);
    void setMarkerTime(std::optional<double> time_s);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Bounds
    {
        double minX = 0.0;
        double maxX = 1.0;
        double minY = 0.0;
        double maxY = 1.0;
    };

    Bounds computeBounds() const;
    QPointF toCanvas(const charts::Point& point, const QRectF& plotRect, const Bounds& bounds) const;
    charts::Theme makeTheme(const QColor& background, const QColor& text) const;
    QColor m_background;
    QColor m_text;
    charts::Module m_module;
    int m_hoveredPoint = -1;
};
