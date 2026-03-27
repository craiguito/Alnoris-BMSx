#include "ChartWidget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace {

QColor colorFromSeries(const charts::Series& series)
{
    return QColor(series.red, series.green, series.blue);
}

} // namespace

ChartWidget::ChartWidget(QWidget* parent)
    : QWidget(parent)
    , m_background(198, 205, 214)
    , m_text(17, 35, 58)
{
    setMinimumHeight(280);
    setMouseTracking(true);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_background);
    setPalette(pal);
}

void ChartWidget::setThemeColors(const QColor& background, const QColor& text)
{
    m_background = background;
    m_text = text;
    m_module.set_theme(makeTheme(background, text));
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_background);
    setPalette(pal);
    update();
}

void ChartWidget::showSingleSeries(const QString& title, const QString& yAxisTitle, const charts::Series& series)
{
    m_module.show_single_series(title.toStdString(), yAxisTitle.toStdString(), series);
    m_module.set_theme(makeTheme(m_background, m_text));
    update();
}

void ChartWidget::showComparison(const QString& title, const QString& yAxisTitle, const charts::Series& baseline, const charts::Series& candidate)
{
    m_module.show_comparison(title.toStdString(), yAxisTitle.toStdString(), baseline, candidate);
    m_module.set_theme(makeTheme(m_background, m_text));
    update();
}

void ChartWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    const charts::ViewModel& vm = m_module.view_model();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(16, 22, 31));

    const QRectF card = rect().adjusted(12, 12, -12, -12);
    painter.setPen(QPen(QColor(vm.theme.border_r, vm.theme.border_g, vm.theme.border_b), 1.0));
    painter.setBrush(m_background);
    painter.drawRoundedRect(card, 20, 20);

    QLinearGradient cardGradient(card.topLeft(), card.bottomLeft());
    cardGradient.setColorAt(0.0, m_background.lighter(103));
    cardGradient.setColorAt(1.0, m_background.darker(106));
    painter.setBrush(cardGradient);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(card.adjusted(1, 1, -1, -1), 20, 20);

    painter.setPen(m_text);
    QFont titleFont = painter.font();
    titleFont.setPixelSize(17);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(card.adjusted(22, 16, -22, 0), QString::fromStdString(vm.title));

    painter.setPen(QColor(115, 133, 154));
    QFont subFont = painter.font();
    subFont.setPixelSize(11);
    subFont.setBold(false);
    painter.setFont(subFont);
    painter.drawText(card.adjusted(22, 40, -22, 0), QString::fromStdString(vm.subtitle));

    QRectF plotRect = card.adjusted(76, 78, -24, -58);
    QLinearGradient plotGradient(plotRect.topLeft(), plotRect.bottomLeft());
    plotGradient.setColorAt(0.0, m_background.lighter(103));
    plotGradient.setColorAt(1.0, m_background.darker(104));
    painter.setPen(Qt::NoPen);
    painter.setBrush(plotGradient);
    painter.drawRect(plotRect);

    const Bounds bounds = computeBounds();
    painter.setPen(QPen(QColor(vm.theme.grid_r, vm.theme.grid_g, vm.theme.grid_b), 1.0));
    for (int i = 0; i <= 5; ++i) {
        const qreal y = plotRect.top() + (plotRect.height() * i / 5.0);
        painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
        const qreal x = plotRect.left() + (plotRect.width() * i / 5.0);
        painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
    }

    painter.setPen(QPen(QColor(108, 122, 138), 1.3));
    painter.drawLine(plotRect.bottomLeft(), plotRect.topLeft());
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());

    painter.setPen(QColor(95, 112, 130));
    painter.setFont(subFont);
    for (int i = 0; i <= 5; ++i) {
        const double value = bounds.maxY - ((bounds.maxY - bounds.minY) * i / 5.0);
        const qreal y = plotRect.top() + (plotRect.height() * i / 5.0);
        painter.drawText(QRectF(plotRect.left() - 58, y - 8, 50, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(value, 'f', 2));
    }
    for (int i = 0; i <= 5; ++i) {
        const double value = bounds.minX + ((bounds.maxX - bounds.minX) * i / 5.0);
        const qreal x = plotRect.left() + (plotRect.width() * i / 5.0);
        painter.drawText(QRectF(x - 20, plotRect.bottom() + 10, 40, 16), Qt::AlignCenter, QString::number(std::round(value)));
    }

    painter.save();
    painter.translate(card.left() + 18, plotRect.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-70, -12, 140, 24), Qt::AlignCenter, QString::fromStdString(vm.y_axis_title));
    painter.restore();
    painter.drawText(QRectF(plotRect.center().x() - 60, card.bottom() - 24, 120, 18), Qt::AlignCenter, QString::fromStdString(vm.x_axis_title));

    auto drawSeries = [&](const charts::Series& series, bool primary) {
        if (!series.visible || series.points.empty()) {
            return;
        }

        QColor lineColor = colorFromSeries(series);
        QPainterPath fillPath;
        QPainterPath linePath;
        QPointF start = toCanvas(series.points.front(), plotRect, bounds);
        linePath.moveTo(start);
        fillPath.moveTo(start.x(), plotRect.bottom());
        fillPath.lineTo(start);

        for (std::size_t i = 1; i < series.points.size(); ++i) {
            const QPointF mapped = toCanvas(series.points[i], plotRect, bounds);
            linePath.lineTo(mapped);
            fillPath.lineTo(mapped);
        }
        const QPointF end = toCanvas(series.points.back(), plotRect, bounds);
        fillPath.lineTo(end.x(), plotRect.bottom());
        fillPath.closeSubpath();

        QColor fillTop = lineColor.lighter(155);
        QColor fillBottom = lineColor.lighter(115);
        fillTop.setAlpha(primary ? 95 : 50);
        fillBottom.setAlpha(primary ? 45 : 25);
        QLinearGradient fillGradient(plotRect.topLeft(), plotRect.bottomLeft());
        fillGradient.setColorAt(0.0, fillTop);
        fillGradient.setColorAt(1.0, fillBottom);
        painter.fillPath(fillPath, fillGradient);

        QPen pen(lineColor, primary ? 3.0 : 2.0);
        if (series.dashed) {
            pen.setStyle(Qt::DashLine);
            pen.setDashPattern({8.0, 6.0});
        }
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(linePath);

        const QPointF endpoint = toCanvas(series.points.back(), plotRect, bounds);
        painter.setBrush(lineColor);
        painter.setPen(QPen(QColor(215, 222, 230), 2.0));
        painter.drawEllipse(endpoint, primary ? 5.0 : 4.0, primary ? 5.0 : 4.0);
    };

    drawSeries(vm.secondary, false);
    drawSeries(vm.primary, true);

    if (m_hoveredPoint >= 0 && m_hoveredPoint < static_cast<int>(vm.primary.points.size())) {
        const charts::Point& point = vm.primary.points[m_hoveredPoint];
        const QPointF hoverPoint = toCanvas(point, plotRect, bounds);
        painter.setPen(QPen(QColor(50, 60, 72, 120), 1.0, Qt::DashLine));
        painter.drawLine(QPointF(hoverPoint.x(), plotRect.top()), QPointF(hoverPoint.x(), plotRect.bottom()));
        painter.drawLine(QPointF(plotRect.left(), hoverPoint.y()), QPointF(plotRect.right(), hoverPoint.y()));

        painter.setPen(QPen(QColor(24, 35, 47), 1.0));
        painter.setBrush(QColor(227, 233, 239));
        QRectF tipRect(hoverPoint.x() + 12, hoverPoint.y() - 40, 120, 34);
        if (tipRect.right() > card.right() - 12) {
            tipRect.moveLeft(hoverPoint.x() - tipRect.width() - 12);
        }
        painter.drawRoundedRect(tipRect, 8, 8);
        painter.setPen(QColor(20, 31, 43));
        painter.drawText(tipRect.adjusted(8, 6, -8, -16), Qt::AlignLeft | Qt::AlignVCenter, QString("t=%1").arg(point.x, 0, 'f', 0));
        painter.drawText(tipRect.adjusted(8, 16, -8, -4), Qt::AlignLeft | Qt::AlignVCenter, QString("y=%1").arg(point.y, 0, 'f', 3));
    }

    QRectF legendPrimary(card.right() - 124, card.top() + 18, 110, 24);
    painter.setPen(QPen(QColor(127, 143, 159), 1.0));
    painter.setBrush(m_background.darker(103));
    painter.drawRoundedRect(legendPrimary, 10, 10);
    painter.fillRect(QRectF(legendPrimary.left() + 10, legendPrimary.center().y() - 1.5, 16, 3), colorFromSeries(vm.primary));
    painter.setPen(QColor(23, 106, 149));
    painter.drawText(legendPrimary.adjusted(34, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, QString::fromStdString(vm.primary.name));

    if (vm.comparison_mode && !vm.secondary.points.empty()) {
        QRectF legendSecondary(card.right() - 248, card.top() + 18, 114, 24);
        painter.setPen(QPen(QColor(127, 143, 159), 1.0));
        painter.setBrush(m_background.darker(103));
        painter.drawRoundedRect(legendSecondary, 10, 10);
        QPen basePen(colorFromSeries(vm.secondary), 2.0);
        basePen.setStyle(Qt::DashLine);
        basePen.setDashPattern({8.0, 6.0});
        painter.setPen(basePen);
        painter.drawLine(QPointF(legendSecondary.left() + 10, legendSecondary.center().y()), QPointF(legendSecondary.left() + 26, legendSecondary.center().y()));
        painter.setPen(QColor(96, 114, 131));
        painter.drawText(legendSecondary.adjusted(34, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, QString::fromStdString(vm.secondary.name));
    }
}

void ChartWidget::mouseMoveEvent(QMouseEvent* event)
{
    const Bounds bounds = computeBounds();
    const charts::ViewModel& vm = m_module.view_model();
    const QRectF card = rect().adjusted(12, 12, -12, -12);
    const QRectF plotRect = card.adjusted(76, 78, -24, -58);
    double bestDistance = 1.0e9;
    int bestIndex = -1;
    for (int i = 0; i < static_cast<int>(vm.primary.points.size()); ++i) {
        const QPointF mapped = toCanvas(vm.primary.points[i], plotRect, bounds);
        const QPointF delta = mapped - event->position();
        const double distance = std::sqrt((delta.x() * delta.x()) + (delta.y() * delta.y()));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    m_hoveredPoint = bestDistance <= 28.0 ? bestIndex : -1;
    update();
    QWidget::mouseMoveEvent(event);
}

void ChartWidget::leaveEvent(QEvent* event)
{
    m_hoveredPoint = -1;
    update();
    QWidget::leaveEvent(event);
}

ChartWidget::Bounds ChartWidget::computeBounds() const
{
    Bounds bounds;
    bool hasPoint = false;
    const charts::ViewModel& vm = m_module.view_model();
    auto visit = [&](const charts::Series& series) {
        for (const charts::Point& point : series.points) {
            if (!hasPoint) {
                bounds.minX = bounds.maxX = point.x;
                bounds.minY = bounds.maxY = point.y;
                hasPoint = true;
            } else {
                bounds.minX = std::min(bounds.minX, point.x);
                bounds.maxX = std::max(bounds.maxX, point.x);
                bounds.minY = std::min(bounds.minY, point.y);
                bounds.maxY = std::max(bounds.maxY, point.y);
            }
        }
    };
    visit(vm.primary);
    visit(vm.secondary);
    if (!hasPoint) {
        return bounds;
    }
    const double yPadding = (bounds.maxY - bounds.minY) == 0.0 ? std::max(1.0, std::abs(bounds.maxY) * 0.05) : (bounds.maxY - bounds.minY) * 0.12;
    bounds.minY -= yPadding;
    bounds.maxY += yPadding;
    if (bounds.maxX <= bounds.minX) {
        bounds.maxX = bounds.minX + 1.0;
    }
    return bounds;
}

QPointF ChartWidget::toCanvas(const charts::Point& point, const QRectF& plotRect, const Bounds& bounds) const
{
    const double xNorm = (point.x - bounds.minX) / std::max(1e-9, bounds.maxX - bounds.minX);
    const double yNorm = (point.y - bounds.minY) / std::max(1e-9, bounds.maxY - bounds.minY);
    return QPointF(
        plotRect.left() + (xNorm * plotRect.width()),
        plotRect.bottom() - (yNorm * plotRect.height())
    );
}

charts::Theme ChartWidget::makeTheme(const QColor& background, const QColor& text) const
{
    charts::Theme theme;
    theme.surface_r = background.red();
    theme.surface_g = background.green();
    theme.surface_b = background.blue();
    theme.text_r = text.red();
    theme.text_g = text.green();
    theme.text_b = text.blue();
    theme.border_r = background.darker(140).red();
    theme.border_g = background.darker(140).green();
    theme.border_b = background.darker(140).blue();
    theme.grid_r = background.darker(125).red();
    theme.grid_g = background.darker(125).green();
    theme.grid_b = background.darker(125).blue();
    return theme;
}
