#include "CadViewportWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <algorithm>

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

    painter.setPen(QColor(90, 102, 116));
    painter.drawText(QRect(16, 12, width() - 32, 20), Qt::AlignLeft | Qt::AlignVCenter, "Standalone CAD module");
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
        m_pressMousePos = event->pos();
        m_lastMousePos = event->pos();
    }
    QWidget::mousePressEvent(event);
}

void CadViewportWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_engine.orbit(static_cast<float>(delta.x()) * 0.45f, static_cast<float>(delta.y()) * 0.30f);
        m_lastMousePos = event->pos();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CadViewportWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && (event->pos() - m_pressMousePos).manhattanLength() < 4) {
        const cad::core::EntityId hit = m_engine.hitTestEntity(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (hit.isValid()) {
            m_engine.selectEntity(hit);
            emit selectionChanged();
            update();
        }
    }
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void CadViewportWidget::wheelEvent(QWheelEvent* event)
{
    m_engine.zoom(event->angleDelta().y() > 0 ? 0.08f : -0.08f);
    update();
    event->accept();
}
