#include "CadViewportWidget.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {

struct ProjectedVertex
{
    QPointF point;
    float depth = 0.0f;
    bool valid = false;
};

struct ProjectedTriangle
{
    QPointF a;
    QPointF b;
    QPointF c;
    QColor color;
    float depth = 0.0f;
};

ProjectedVertex projectPoint(const std::array<float, 16>& mvp, const cad::math::Vec3& p, int width, int height)
{
    const float x = p.x;
    const float y = p.y;
    const float z = p.z;
    const float w = 1.0f;

    const float clipX = (mvp[0] * x) + (mvp[1] * y) + (mvp[2] * z) + (mvp[3] * w);
    const float clipY = (mvp[4] * x) + (mvp[5] * y) + (mvp[6] * z) + (mvp[7] * w);
    const float clipZ = (mvp[8] * x) + (mvp[9] * y) + (mvp[10] * z) + (mvp[11] * w);
    const float clipW = (mvp[12] * x) + (mvp[13] * y) + (mvp[14] * z) + (mvp[15] * w);

    if (clipW <= 0.0001f) {
        return {};
    }

    const float ndcX = clipX / clipW;
    const float ndcY = clipY / clipW;
    const float ndcZ = clipZ / clipW;
    return {
        QPointF((ndcX * 0.5f + 0.5f) * width, (1.0f - (ndcY * 0.5f + 0.5f)) * height),
        ndcZ,
        true
    };
}

QColor toColor(const cad::math::Vec3& color)
{
    return QColor::fromRgbF(
        std::clamp(color.x, 0.0f, 1.0f),
        std::clamp(color.y, 0.0f, 1.0f),
        std::clamp(color.z, 0.0f, 1.0f)
    );
}

void drawSelectionOverlay(QPainter& painter, const cad::render::ScreenPickable& pickable)
{
    const QColor glow(47, 128, 237, 90);
    const QColor outline(34, 120, 255);
    const QColor inner(142, 198, 255);

    painter.save();
    if (pickable.shape == cad::render::ScreenPickable::Shape::Circle) {
        const QRectF outerRect(
            pickable.x - pickable.half_width - 8.0f,
            pickable.y - pickable.half_height - 8.0f,
            (pickable.half_width + 8.0f) * 2.0f,
            (pickable.half_height + 8.0f) * 2.0f
        );
        painter.setPen(QPen(glow, 10.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(outerRect);
        painter.setPen(QPen(outline, 4.0));
        painter.drawEllipse(outerRect.adjusted(2.0, 2.0, -2.0, -2.0));
        painter.setPen(QPen(inner, 1.5));
        painter.drawEllipse(outerRect.adjusted(8.0, 8.0, -8.0, -8.0));
    } else {
        const QRectF outerRect(
            pickable.x - pickable.half_width - 8.0f,
            pickable.y - pickable.half_height - 8.0f,
            (pickable.half_width + 8.0f) * 2.0f,
            (pickable.half_height + 8.0f) * 2.0f
        );
        painter.setPen(QPen(glow, 10.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(outerRect, 10.0, 10.0);
        painter.setPen(QPen(outline, 4.0));
        painter.drawRoundedRect(outerRect.adjusted(2.0, 2.0, -2.0, -2.0), 8.0, 8.0);
        painter.setPen(QPen(inner, 1.5));
        painter.drawRoundedRect(outerRect.adjusted(8.0, 8.0, -8.0, -8.0), 6.0, 6.0);
    }
    painter.restore();
}

float snapCoordinate(float value, float step)
{
    if (step <= 0.0f) {
        return value;
    }
    return std::round(value / step) * step;
}

} // namespace

CadViewportWidget::CadViewportWidget(QWidget* parent)
    : QWidget(parent)
    , m_backgroundColor(198, 205, 214)
{
    setMinimumHeight(540);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_backgroundColor);
    setPalette(pal);
}

bool CadViewportWidget::setCellMeshPath(const QString& path)
{
    const bool loaded = m_engine.setCellMeshPath(path.toStdString());
    update();
    return loaded;
}

void CadViewportWidget::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_backgroundColor);
    setPalette(pal);
    update();
}

void CadViewportWidget::setPackConfig(const cad::battery::BatteryCadConfig& config)
{
    m_engine.setBatteryConfig(config);
    emit selectionChanged();
    update();
}

void CadViewportWidget::setSimulationOverlay(const cad::battery::BatteryVisualizationOverlay& overlay)
{
    m_engine.setVisualizationOverlay(overlay);
    update();
}

void CadViewportWidget::clearSimulationOverlay()
{
    m_engine.clearVisualizationOverlay();
    update();
}

const cad::core::CadDocument& CadViewportWidget::document() const
{
    return m_engine.document();
}

std::optional<cad::battery::EntitySummary> CadViewportWidget::selectedEntitySummary() const
{
    return m_engine.getSelectedEntitySummary();
}

std::optional<cad::battery::CellProperties> CadViewportWidget::selectedCellProperties() const
{
    return m_engine.getCellProperties(m_engine.selectedEntity());
}

std::optional<cad::battery::BusbarProperties> CadViewportWidget::selectedBusbarProperties() const
{
    return m_engine.getBusbarProperties(m_engine.selectedEntity());
}

std::optional<cad::battery::CoolingPlateProperties> CadViewportWidget::selectedCoolingPlateProperties() const
{
    return m_engine.getCoolingPlateProperties(m_engine.selectedEntity());
}

std::optional<cad::battery::ModuleBoundaryProperties> CadViewportWidget::selectedModuleBoundaryProperties() const
{
    return m_engine.getModuleBoundaryProperties(m_engine.selectedEntity());
}

std::optional<cad::battery::PackEnclosureProperties> CadViewportWidget::selectedEnclosureProperties() const
{
    return m_engine.getEnclosureProperties(m_engine.selectedEntity());
}

bool CadViewportWidget::applyRenameToSelected(const QString& label)
{
    const bool changed = m_engine.applyRenameEntity(m_engine.selectedEntity(), label.toStdString());
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::applySelectedVisibility(bool visible)
{
    const bool changed = m_engine.applySetEntityVisibility(m_engine.selectedEntity(), visible);
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::applySelectedCellUpdate(const cad::battery::CellPropertiesUpdate& updateData)
{
    const bool changed = m_engine.applyCellPropertiesUpdate(m_engine.selectedEntity(), updateData);
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::applySelectedBusbarUpdate(const cad::battery::BusbarPropertiesUpdate& updateData)
{
    const bool changed = m_engine.applyBusbarPropertiesUpdate(m_engine.selectedEntity(), updateData);
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::applySelectedCoolingPlateUpdate(const cad::battery::CoolingPlatePropertiesUpdate& updateData)
{
    const bool changed = m_engine.applyCoolingPlatePropertiesUpdate(m_engine.selectedEntity(), updateData);
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::applySelectedModuleBoundaryUpdate(const cad::battery::ModuleBoundaryPropertiesUpdate& updateData)
{
    const bool changed = m_engine.applyModuleBoundaryPropertiesUpdate(m_engine.selectedEntity(), updateData);
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::applySelectedEnclosureUpdate(const cad::battery::PackEnclosurePropertiesUpdate& updateData)
{
    const bool changed = m_engine.applyEnclosurePropertiesUpdate(m_engine.selectedEntity(), updateData);
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::undoLastEdit()
{
    const bool changed = m_engine.undo();
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::redoLastEdit()
{
    const bool changed = m_engine.redo();
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::resetSelectedPositionToGenerated()
{
    const bool changed = m_engine.applyResetEntityPositionToGenerated(m_engine.selectedEntity());
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::resetSelectedGeometryToGenerated()
{
    const bool changed = m_engine.applyResetEntityGeometryToGenerated(m_engine.selectedEntity());
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

bool CadViewportWidget::resetSelectedLabelToGenerated()
{
    const bool changed = m_engine.applyResetEntityLabelToGenerated(m_engine.selectedEntity());
    if (changed) {
        emit selectionChanged();
        update();
    }
    return changed;
}

void CadViewportWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), m_backgroundColor);

    const cad::render::RenderPacket& frame = m_engine.renderPacket();
    if (frame.triangles.empty() && frame.lines.empty()) {
        painter.setPen(QColor(90, 102, 116));
        painter.drawText(rect(), Qt::AlignCenter, "CAD viewport has no scene geometry");
        return;
    }

    std::vector<ProjectedTriangle> projectedTriangles;
    projectedTriangles.reserve(frame.triangles.size() / 3);
    for (std::size_t i = 0; i + 2 < frame.triangles.size(); i += 3) {
        const ProjectedVertex a = projectPoint(frame.mvp, frame.triangles[i].position, width(), height());
        const ProjectedVertex b = projectPoint(frame.mvp, frame.triangles[i + 1].position, width(), height());
        const ProjectedVertex c = projectPoint(frame.mvp, frame.triangles[i + 2].position, width(), height());
        if (!a.valid || !b.valid || !c.valid) {
            continue;
        }

        const cad::math::Vec3 avgColor{
            (frame.triangles[i].color.x + frame.triangles[i + 1].color.x + frame.triangles[i + 2].color.x) / 3.0f,
            (frame.triangles[i].color.y + frame.triangles[i + 1].color.y + frame.triangles[i + 2].color.y) / 3.0f,
            (frame.triangles[i].color.z + frame.triangles[i + 1].color.z + frame.triangles[i + 2].color.z) / 3.0f
        };

        projectedTriangles.push_back({
            a.point,
            b.point,
            c.point,
            toColor(avgColor),
            (a.depth + b.depth + c.depth) / 3.0f
        });
    }

    std::sort(projectedTriangles.begin(), projectedTriangles.end(), [](const ProjectedTriangle& lhs, const ProjectedTriangle& rhs) {
        return lhs.depth > rhs.depth;
    });

    painter.setPen(Qt::NoPen);
    for (const ProjectedTriangle& tri : projectedTriangles) {
        QPainterPath path;
        path.moveTo(tri.a);
        path.lineTo(tri.b);
        path.lineTo(tri.c);
        path.closeSubpath();
        painter.fillPath(path, tri.color);
    }

    painter.setPen(QPen(QColor(156, 166, 178), 1.0));
    for (std::size_t i = 0; i + 1 < frame.lines.size(); i += 2) {
        const ProjectedVertex a = projectPoint(frame.mvp, frame.lines[i].position, width(), height());
        const ProjectedVertex b = projectPoint(frame.mvp, frame.lines[i + 1].position, width(), height());
        if (!a.valid || !b.valid) {
            continue;
        }
        painter.setPen(QPen(toColor(frame.lines[i].color), 1.0));
        painter.drawLine(a.point, b.point);
    }

    if (!frame.selection_overlay_lines.empty()) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (int pass = 0; pass < 2; ++pass) {
            const QColor lineColor = pass == 0 ? QColor(46, 126, 255, 85) : QColor(32, 120, 255);
            const qreal lineWidth = pass == 0 ? 6.0 : 2.4;
            painter.setPen(QPen(lineColor, lineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            for (std::size_t i = 0; i + 1 < frame.selection_overlay_lines.size(); i += 2) {
                const ProjectedVertex a = projectPoint(frame.mvp, frame.selection_overlay_lines[i].position, width(), height());
                const ProjectedVertex b = projectPoint(frame.mvp, frame.selection_overlay_lines[i + 1].position, width(), height());
                if (!a.valid || !b.valid) {
                    continue;
                }
                painter.drawLine(a.point, b.point);
            }
        }
    }

    const cad::core::EntityId selectedId = m_engine.selectedEntity();
    if (selectedId.isValid()) {
        const auto selectedPickable = std::find_if(
            frame.pickables.rbegin(),
            frame.pickables.rend(),
            [selectedId](const cad::render::ScreenPickable& pickable) { return pickable.entity_id == selectedId; }
        );
        if (selectedPickable != frame.pickables.rend()) {
            drawSelectionOverlay(painter, *selectedPickable);
        }
    }

    painter.setPen(QColor(90, 102, 116));
    painter.drawText(
        QRect(16, 12, width() - 32, 20),
        Qt::AlignLeft | Qt::AlignVCenter,
        QString("Battery CAD viewport  |  Shift-drag move  |  G snap %1  |  Axis %2")
            .arg(m_gridSnapEnabled ? "on" : "off")
            .arg(m_moveAxis == MoveAxis::X ? "X" : m_moveAxis == MoveAxis::Y ? "Y" : m_moveAxis == MoveAxis::Z ? "Z" : "XZ")
    );
}

void CadViewportWidget::resizeEvent(QResizeEvent* event)
{
    m_engine.setViewportSize(event->size().width(), event->size().height());
    QWidget::resizeEvent(event);
}

void CadViewportWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_moveDragging = event->modifiers().testFlag(Qt::ShiftModifier) && m_engine.selectedEntity().isValid();
        m_pressMousePos = event->pos();
        m_lastMousePos = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void CadViewportWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->pos() - m_lastMousePos;
        if (m_moveDragging) {
            cad::math::Vec3 moveDelta{};
            const float moveScale = 0.8f;
            switch (m_moveAxis) {
            case MoveAxis::X:
                moveDelta.x = static_cast<float>(delta.x()) * moveScale;
                break;
            case MoveAxis::Y:
                moveDelta.y = static_cast<float>(-delta.y()) * moveScale;
                break;
            case MoveAxis::Z:
                moveDelta.z = static_cast<float>(delta.y()) * moveScale;
                break;
            case MoveAxis::FreeXZ:
            default:
                moveDelta.x = static_cast<float>(delta.x()) * moveScale;
                moveDelta.z = static_cast<float>(delta.y()) * moveScale;
                break;
            }
            if (m_gridSnapEnabled) {
                const cad::core::EntityId selectedId = m_engine.selectedEntity();
                const cad::math::Vec3 currentPosition = m_engine.document().worldPosition(selectedId);
                cad::math::Vec3 snappedTarget = cad::math::add(currentPosition, moveDelta);
                snappedTarget.x = snapCoordinate(snappedTarget.x, m_gridSnapStep);
                snappedTarget.y = snapCoordinate(snappedTarget.y, m_gridSnapStep);
                snappedTarget.z = snapCoordinate(snappedTarget.z, m_gridSnapStep);
                moveDelta = {
                    snappedTarget.x - currentPosition.x,
                    snappedTarget.y - currentPosition.y,
                    snappedTarget.z - currentPosition.z
                };
            }
            m_engine.moveEntity(m_engine.selectedEntity(), moveDelta);
        } else {
            m_engine.orbit(static_cast<float>(delta.x()) * 0.45f, static_cast<float>(delta.y()) * 0.30f);
        }
        m_lastMousePos = event->pos();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CadViewportWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && !m_moveDragging && (event->pos() - m_pressMousePos).manhattanLength() < 4) {
        const cad::core::EntityId hit = m_engine.hitTestEntity(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (hit.isValid()) {
            m_engine.selectEntity(hit);
        } else {
            m_engine.clearSelection();
        }
        emit selectionChanged();
        update();
    }
    m_dragging = false;
    m_moveDragging = false;
    QWidget::mouseReleaseEvent(event);
}

void CadViewportWidget::wheelEvent(QWheelEvent* event)
{
    m_engine.zoom(event->angleDelta().y() > 0 ? 0.08f : -0.08f);
    update();
    event->accept();
}

void CadViewportWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_G:
        m_gridSnapEnabled = !m_gridSnapEnabled;
        event->accept();
        return;
    case Qt::Key_X:
        m_moveAxis = MoveAxis::X;
        event->accept();
        return;
    case Qt::Key_Y:
        m_moveAxis = MoveAxis::Y;
        event->accept();
        return;
    case Qt::Key_Z:
        m_moveAxis = MoveAxis::Z;
        event->accept();
        return;
    case Qt::Key_A:
        m_moveAxis = MoveAxis::FreeXZ;
        event->accept();
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}
