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

ProjectedVertex projectPoint(const std::array<float, 16>& mvp, const cad::Vec3& p, int width, int height)
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

QColor toColor(const cad::Vec3& color)
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
    const bool loaded = m_module.set_cell_mesh_path(path.toStdString());
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

void CadViewportWidget::setPackConfig(const cad::PackConfig& config)
{
    m_module.set_pack_config(config);
    update();
}

void CadViewportWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), m_backgroundColor);

    const cad::FrameData& frame = m_module.frame_data();
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

        const cad::Vec3 avgColor{
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
    m_module.set_viewport_size(event->size().width(), event->size().height());
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
        m_module.orbit(static_cast<float>(delta.x()) * 0.45f, static_cast<float>(delta.y()) * 0.30f);
        m_lastMousePos = event->pos();
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void CadViewportWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && (event->pos() - m_pressMousePos).manhattanLength() < 4) {
        const int hit = m_module.hit_test_cell(static_cast<float>(event->position().x()), static_cast<float>(event->position().y()));
        if (hit >= 0) {
            m_module.select_cell(hit);
            update();
        }
    }
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void CadViewportWidget::wheelEvent(QWheelEvent* event)
{
    m_module.zoom(event->angleDelta().y() > 0 ? 0.08f : -0.08f);
    update();
    event->accept();
}
