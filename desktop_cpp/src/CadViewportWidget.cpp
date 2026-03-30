#include "CadViewportWidget.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

using cad::math::Vec3;

struct GpuVertexView
{
    static constexpr int PositionOffset = 0;
    static constexpr int ColorOffset = sizeof(float) * 3;
    static constexpr int NormalOffset = sizeof(float) * 6;
    static constexpr int AlphaOffset = sizeof(float) * 9;
    static constexpr int Stride = sizeof(float) * 10;
};

struct GpuLineView
{
    static constexpr int PositionOffset = 0;
    static constexpr int ColorOffset = sizeof(float) * 3;
    static constexpr int AlphaOffset = sizeof(float) * 6;
    static constexpr int Stride = sizeof(float) * 7;
};

struct GpuTriangleVertex
{
    float position[3]{};
    float color[3]{};
    float normal[3]{};
    float alpha = 1.0f;
};

struct GpuLineVertex
{
    float position[3]{};
    float color[3]{};
    float alpha = 1.0f;
};

Vec3 subtractVec(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 crossVec(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dotVec(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float lengthVec(const Vec3& v)
{
    return std::sqrt(dotVec(v, v));
}

Vec3 normalizeVec(const Vec3& v)
{
    const float length = lengthVec(v);
    if (length <= 0.0001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {v.x / length, v.y / length, v.z / length};
}

float triangleLayerAlpha(unsigned char layer)
{
    switch (layer) {
    case 0:
        return 0.0f;
    case 1:
        return 1.0f;
    case 3:
        return 1.0f;
    case 2:
    default:
        return 1.0f;
    }
}

bool shouldRenderTriangleLayer(unsigned char layer)
{
    return triangleLayerAlpha(layer) > 0.001f;
}

bool isOpaqueTriangleLayer(unsigned char layer)
{
    return triangleLayerAlpha(layer) >= 0.999f;
}

float lineLayerAlpha(unsigned char layer)
{
    switch (layer) {
    case 0:
        return 0.28f;
    case 1:
        return 0.48f;
    default:
        return 0.78f;
    }
}

QColor hudTextColor()
{
    return QColor(92, 102, 114);
}

void drawSelectionOverlay(QPainter& painter, const cad::render::ScreenPickable& pickable)
{
    const QColor outline(31, 116, 247);

    painter.save();
    painter.setPen(QPen(outline, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    if (pickable.shape == cad::render::ScreenPickable::Shape::Circle) {
        const QRectF outlineRect(
            pickable.x - pickable.half_width - 6.0f,
            pickable.y - pickable.half_height - 6.0f,
            (pickable.half_width + 6.0f) * 2.0f,
            (pickable.half_height + 6.0f) * 2.0f
        );
        painter.drawEllipse(outlineRect);
    } else {
        const QRectF outlineRect(
            pickable.x - pickable.half_width - 6.0f,
            pickable.y - pickable.half_height - 6.0f,
            (pickable.half_width + 6.0f) * 2.0f,
            (pickable.half_height + 6.0f) * 2.0f
        );
        painter.drawRoundedRect(outlineRect, 8.0, 8.0);
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

GpuTriangleVertex makeTriangleVertex(
    const Vec3& position,
    const Vec3& color,
    const Vec3& normal,
    float alpha
)
{
    GpuTriangleVertex vertex;
    vertex.position[0] = position.x;
    vertex.position[1] = position.y;
    vertex.position[2] = position.z;
    vertex.color[0] = color.x;
    vertex.color[1] = color.y;
    vertex.color[2] = color.z;
    vertex.normal[0] = normal.x;
    vertex.normal[1] = normal.y;
    vertex.normal[2] = normal.z;
    vertex.alpha = alpha;
    return vertex;
}

GpuLineVertex makeLineVertex(const Vec3& position, const Vec3& color, float alpha)
{
    GpuLineVertex vertex;
    vertex.position[0] = position.x;
    vertex.position[1] = position.y;
    vertex.position[2] = position.z;
    vertex.color[0] = color.x;
    vertex.color[1] = color.y;
    vertex.color[2] = color.z;
    vertex.alpha = alpha;
    return vertex;
}

std::vector<GpuTriangleVertex> buildTriangleVertices(const cad::geometry::GeometryBuffer& geometry)
{
    std::vector<GpuTriangleVertex> vertices;
    vertices.reserve(geometry.triangles.size() * 3);
    for (const cad::geometry::ColoredTriangle& triangle : geometry.triangles) {
        const unsigned char layer = static_cast<unsigned char>(triangle.layer);
        if (!shouldRenderTriangleLayer(layer) || !isOpaqueTriangleLayer(layer)) {
            continue;
        }
        const Vec3 edge_ab = subtractVec(triangle.b, triangle.a);
        const Vec3 edge_ac = subtractVec(triangle.c, triangle.a);
        const Vec3 normal = normalizeVec(crossVec(edge_ab, edge_ac));
        const float alpha = triangleLayerAlpha(layer);
        vertices.push_back(makeTriangleVertex(triangle.a, triangle.color, normal, alpha));
        vertices.push_back(makeTriangleVertex(triangle.b, triangle.color, normal, alpha));
        vertices.push_back(makeTriangleVertex(triangle.c, triangle.color, normal, alpha));
    }
    return vertices;
}

float clipDepthForPoint(const std::array<float, 16>& mvp, const Vec3& point)
{
    cad::math::Mat4 matrix{};
    matrix.m = mvp;
    const cad::math::Vec4 clip = cad::math::multiply(matrix, cad::math::Vec4{point.x, point.y, point.z, 1.0f});
    if (std::abs(clip.w) <= 0.0001f) {
        return clip.z;
    }
    return clip.z / clip.w;
}

std::vector<GpuTriangleVertex> buildTransparentTriangleVertices(
    const cad::geometry::GeometryBuffer& geometry,
    const std::array<float, 16>& mvp
)
{
    struct TransparentTriangle
    {
        cad::geometry::ColoredTriangle triangle{};
        float depth = 0.0f;
    };

    std::vector<TransparentTriangle> sorted;
    sorted.reserve(geometry.triangles.size());
    for (const cad::geometry::ColoredTriangle& triangle : geometry.triangles) {
        const unsigned char layer = static_cast<unsigned char>(triangle.layer);
        if (!shouldRenderTriangleLayer(layer) || isOpaqueTriangleLayer(layer)) {
            continue;
        }
        const Vec3 centroid{
            (triangle.a.x + triangle.b.x + triangle.c.x) / 3.0f,
            (triangle.a.y + triangle.b.y + triangle.c.y) / 3.0f,
            (triangle.a.z + triangle.b.z + triangle.c.z) / 3.0f
        };
        sorted.push_back({triangle, clipDepthForPoint(mvp, centroid)});
    }

    std::sort(sorted.begin(), sorted.end(), [](const TransparentTriangle& a, const TransparentTriangle& b) {
        return a.depth > b.depth;
    });

    std::vector<GpuTriangleVertex> vertices;
    vertices.reserve(sorted.size() * 3);
    for (const TransparentTriangle& entry : sorted) {
        const cad::geometry::ColoredTriangle& triangle = entry.triangle;
        const Vec3 edge_ab = subtractVec(triangle.b, triangle.a);
        const Vec3 edge_ac = subtractVec(triangle.c, triangle.a);
        const Vec3 normal = normalizeVec(crossVec(edge_ab, edge_ac));
        const float alpha = triangleLayerAlpha(static_cast<unsigned char>(triangle.layer));
        vertices.push_back(makeTriangleVertex(triangle.a, triangle.color, normal, alpha));
        vertices.push_back(makeTriangleVertex(triangle.b, triangle.color, normal, alpha));
        vertices.push_back(makeTriangleVertex(triangle.c, triangle.color, normal, alpha));
    }
    return vertices;
}

std::vector<GpuTriangleVertex> buildSortedTriangleVertices(
    const cad::geometry::GeometryBuffer& geometry,
    const std::array<float, 16>& mvp
)
{
    struct SortedTriangle
    {
        cad::geometry::ColoredTriangle triangle{};
        float depth = 0.0f;
    };

    std::vector<SortedTriangle> sorted;
    sorted.reserve(geometry.triangles.size());
    for (const cad::geometry::ColoredTriangle& triangle : geometry.triangles) {
        const unsigned char layer = static_cast<unsigned char>(triangle.layer);
        if (!shouldRenderTriangleLayer(layer)) {
            continue;
        }
        const Vec3 centroid{
            (triangle.a.x + triangle.b.x + triangle.c.x) / 3.0f,
            (triangle.a.y + triangle.b.y + triangle.c.y) / 3.0f,
            (triangle.a.z + triangle.b.z + triangle.c.z) / 3.0f
        };
        sorted.push_back({triangle, clipDepthForPoint(mvp, centroid)});
    }

    std::sort(sorted.begin(), sorted.end(), [](const SortedTriangle& a, const SortedTriangle& b) {
        return a.depth > b.depth;
    });

    std::vector<GpuTriangleVertex> vertices;
    vertices.reserve(sorted.size() * 3);
    for (const SortedTriangle& entry : sorted) {
        const cad::geometry::ColoredTriangle& triangle = entry.triangle;
        const Vec3 edge_ab = subtractVec(triangle.b, triangle.a);
        const Vec3 edge_ac = subtractVec(triangle.c, triangle.a);
        const Vec3 normal = normalizeVec(crossVec(edge_ab, edge_ac));
        const float alpha = triangleLayerAlpha(static_cast<unsigned char>(triangle.layer));
        vertices.push_back(makeTriangleVertex(triangle.a, triangle.color, normal, alpha));
        vertices.push_back(makeTriangleVertex(triangle.b, triangle.color, normal, alpha));
        vertices.push_back(makeTriangleVertex(triangle.c, triangle.color, normal, alpha));
    }
    return vertices;
}

std::vector<GpuLineVertex> buildSceneLineVertices(const cad::geometry::GeometryBuffer& geometry)
{
    std::vector<GpuLineVertex> vertices;
    vertices.reserve(geometry.lines.size() * 2);
    for (const cad::geometry::ColoredLine& line : geometry.lines) {
        const float alpha = lineLayerAlpha(static_cast<unsigned char>(line.layer));
        vertices.push_back(makeLineVertex(line.a, line.color, alpha));
        vertices.push_back(makeLineVertex(line.b, line.color, alpha));
    }
    return vertices;
}

std::vector<GpuLineVertex> buildPacketLineVertices(
    const std::vector<cad::render::RenderVertex>& vertices_in,
    float forced_alpha = -1.0f
)
{
    std::vector<GpuLineVertex> vertices;
    vertices.reserve(vertices_in.size());
    for (const cad::render::RenderVertex& vertex : vertices_in) {
        const float alpha = forced_alpha >= 0.0f ? forced_alpha : lineLayerAlpha(vertex.layer);
        vertices.push_back(makeLineVertex(vertex.position, vertex.color, alpha));
    }
    return vertices;
}

constexpr const char* kTriangleVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in float aAlpha;

uniform mat4 uMvp;

out vec3 vColor;
out vec3 vNormal;
out float vAlpha;

void main()
{
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vColor = aColor;
    vNormal = aNormal;
    vAlpha = aAlpha;
}
)";

constexpr const char* kTriangleFragmentShader = R"(
#version 330 core
in vec3 vColor;
in vec3 vNormal;
in float vAlpha;

out vec4 fragColor;

void main()
{
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.42, 0.85, 0.31));
    float lambert = max(dot(normal, lightDir), 0.0);
    float shade = clamp(0.56 + 0.44 * lambert, 0.0, 1.15);
    fragColor = vec4(clamp(vColor * shade, 0.0, 1.0), vAlpha);
}
)";

constexpr const char* kLineVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
layout(location = 2) in float aAlpha;

uniform mat4 uMvp;

out vec3 vColor;
out float vAlpha;

void main()
{
    gl_Position = uMvp * vec4(aPosition, 1.0);
    vColor = aColor;
    vAlpha = aAlpha;
}
)";

constexpr const char* kLineFragmentShader = R"(
#version 330 core
in vec3 vColor;
in float vAlpha;

out vec4 fragColor;

void main()
{
    fragColor = vec4(vColor, vAlpha);
}
)";

} // namespace

CadViewportWidget::CadViewportWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_backgroundColor(198, 205, 214)
    , m_opaqueTriangleBuffer(QOpenGLBuffer::VertexBuffer)
    , m_translucentTriangleBuffer(QOpenGLBuffer::VertexBuffer)
    , m_sceneLineBuffer(QOpenGLBuffer::VertexBuffer)
    , m_overlayLineBuffer(QOpenGLBuffer::VertexBuffer)
    , m_selectionLineBuffer(QOpenGLBuffer::VertexBuffer)
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setAlphaBufferSize(8);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSamples(4);
    setFormat(format);

    setMinimumHeight(540);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

CadViewportWidget::~CadViewportWidget()
{
    if (context() != nullptr) {
        makeCurrent();
        destroyGlResources();
        doneCurrent();
    }
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

void CadViewportWidget::initializeGL()
{
    initializeOpenGLFunctions();
    ensureGpuResources();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    GLint depthBits = 0;
    glGetIntegerv(GL_DEPTH_BITS, &depthBits);
    m_depthBits = static_cast<int>(depthBits);
}

void CadViewportWidget::resizeGL(int w, int h)
{
    m_engine.setViewportSize(w, h);
    m_sceneFramebuffer.reset();
}

void CadViewportWidget::paintGL()
{
    ensureGpuResources();
    ensureSceneFramebuffer();
    syncGpuBuffers();

    const auto drawStart = std::chrono::steady_clock::now();

    const int framebufferWidth = m_sceneFramebuffer != nullptr ? m_sceneFramebuffer->width() : width();
    const int framebufferHeight = m_sceneFramebuffer != nullptr ? m_sceneFramebuffer->height() : height();

    if (m_sceneFramebuffer != nullptr) {
        m_sceneFramebuffer->bind();
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    }

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(
        static_cast<float>(m_backgroundColor.redF()),
        static_cast<float>(m_backgroundColor.greenF()),
        static_cast<float>(m_backgroundColor.blueF()),
        1.0f
    );
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const cad::render::RenderPacket& frame = m_engine.renderPacket();
    const bool sortedTriangleFallback = m_depthBits <= 0;

    if (m_vao.isCreated()) {
        m_vao.bind();
    }

    if (m_triangleProgram != nullptr) {
        m_triangleProgram->bind();
        glUniformMatrix4fv(m_triangleProgram->uniformLocation("uMvp"), 1, GL_TRUE, frame.mvp.data());

        if (sortedTriangleFallback) {
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            if (m_translucentTriangleVertexCount > 0) {
                m_translucentTriangleBuffer.bind();
                m_triangleProgram->enableAttributeArray(0);
                m_triangleProgram->enableAttributeArray(1);
                m_triangleProgram->enableAttributeArray(2);
                m_triangleProgram->enableAttributeArray(3);
                m_triangleProgram->setAttributeBuffer(0, GL_FLOAT, GpuVertexView::PositionOffset, 3, GpuVertexView::Stride);
                m_triangleProgram->setAttributeBuffer(1, GL_FLOAT, GpuVertexView::ColorOffset, 3, GpuVertexView::Stride);
                m_triangleProgram->setAttributeBuffer(2, GL_FLOAT, GpuVertexView::NormalOffset, 3, GpuVertexView::Stride);
                m_triangleProgram->setAttributeBuffer(3, GL_FLOAT, GpuVertexView::AlphaOffset, 1, GpuVertexView::Stride);
                glDrawArrays(GL_TRIANGLES, 0, m_translucentTriangleVertexCount);
                m_translucentTriangleBuffer.release();
            }
        } else if (m_opaqueTriangleVertexCount > 0) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            m_opaqueTriangleBuffer.bind();
            m_triangleProgram->enableAttributeArray(0);
            m_triangleProgram->enableAttributeArray(1);
            m_triangleProgram->enableAttributeArray(2);
            m_triangleProgram->enableAttributeArray(3);
            m_triangleProgram->setAttributeBuffer(0, GL_FLOAT, GpuVertexView::PositionOffset, 3, GpuVertexView::Stride);
            m_triangleProgram->setAttributeBuffer(1, GL_FLOAT, GpuVertexView::ColorOffset, 3, GpuVertexView::Stride);
            m_triangleProgram->setAttributeBuffer(2, GL_FLOAT, GpuVertexView::NormalOffset, 3, GpuVertexView::Stride);
            m_triangleProgram->setAttributeBuffer(3, GL_FLOAT, GpuVertexView::AlphaOffset, 1, GpuVertexView::Stride);
            glDrawArrays(GL_TRIANGLES, 0, m_opaqueTriangleVertexCount);
            m_opaqueTriangleBuffer.release();
        }

        if (m_translucentTriangleVertexCount > 0) {
            glEnable(GL_BLEND);
            glDepthMask(GL_FALSE);
            m_translucentTriangleBuffer.bind();
            m_triangleProgram->enableAttributeArray(0);
            m_triangleProgram->enableAttributeArray(1);
            m_triangleProgram->enableAttributeArray(2);
            m_triangleProgram->enableAttributeArray(3);
            m_triangleProgram->setAttributeBuffer(0, GL_FLOAT, GpuVertexView::PositionOffset, 3, GpuVertexView::Stride);
            m_triangleProgram->setAttributeBuffer(1, GL_FLOAT, GpuVertexView::ColorOffset, 3, GpuVertexView::Stride);
            m_triangleProgram->setAttributeBuffer(2, GL_FLOAT, GpuVertexView::NormalOffset, 3, GpuVertexView::Stride);
            m_triangleProgram->setAttributeBuffer(3, GL_FLOAT, GpuVertexView::AlphaOffset, 1, GpuVertexView::Stride);
            glDrawArrays(GL_TRIANGLES, 0, m_translucentTriangleVertexCount);
            m_translucentTriangleBuffer.release();
        }

        glDepthMask(GL_TRUE);
        glEnable(GL_BLEND);
        m_triangleProgram->disableAttributeArray(0);
        m_triangleProgram->disableAttributeArray(1);
        m_triangleProgram->disableAttributeArray(2);
        m_triangleProgram->disableAttributeArray(3);
        m_triangleProgram->release();
    }

    if (m_lineProgram != nullptr) {
        m_lineProgram->bind();
        glUniformMatrix4fv(m_lineProgram->uniformLocation("uMvp"), 1, GL_TRUE, frame.mvp.data());

        const auto drawLineBuffer = [this](QOpenGLBuffer& buffer, int vertex_count, bool depth_test, float width) {
            if (vertex_count <= 0) {
                return;
            }
            if (depth_test) {
                glEnable(GL_DEPTH_TEST);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
            glLineWidth(width);
            buffer.bind();
            m_lineProgram->enableAttributeArray(0);
            m_lineProgram->enableAttributeArray(1);
            m_lineProgram->enableAttributeArray(2);
            m_lineProgram->setAttributeBuffer(0, GL_FLOAT, GpuLineView::PositionOffset, 3, GpuLineView::Stride);
            m_lineProgram->setAttributeBuffer(1, GL_FLOAT, GpuLineView::ColorOffset, 3, GpuLineView::Stride);
            m_lineProgram->setAttributeBuffer(2, GL_FLOAT, GpuLineView::AlphaOffset, 1, GpuLineView::Stride);
            glDrawArrays(GL_LINES, 0, vertex_count);
            m_lineProgram->disableAttributeArray(0);
            m_lineProgram->disableAttributeArray(1);
            m_lineProgram->disableAttributeArray(2);
            buffer.release();
        };

        if (!sortedTriangleFallback) {
            drawLineBuffer(m_sceneLineBuffer, m_sceneLineVertexCount, true, 1.0f);
        }
        drawLineBuffer(m_overlayLineBuffer, m_overlayLineVertexCount, true, 1.0f);
        drawLineBuffer(m_selectionLineBuffer, m_selectionLineVertexCount, false, 2.0f);
        glEnable(GL_DEPTH_TEST);
        m_lineProgram->release();
    }

    if (m_vao.isCreated()) {
        m_vao.release();
    }

    if (m_sceneFramebuffer != nullptr) {
        m_sceneFramebuffer->release();
        if (QOpenGLContext* currentContext = context(); currentContext != nullptr) {
            QOpenGLExtraFunctions* extra = currentContext->extraFunctions();
            extra->glBindFramebuffer(GL_READ_FRAMEBUFFER, m_sceneFramebuffer->handle());
            extra->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFramebufferObject());
            extra->glBlitFramebuffer(
                0,
                0,
                framebufferWidth,
                framebufferHeight,
                0,
                0,
                framebufferWidth,
                framebufferHeight,
                GL_COLOR_BUFFER_BIT,
                GL_NEAREST
            );
        }
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    }

    const auto drawEnd = std::chrono::steady_clock::now();
    m_lastGpuDrawMs = std::chrono::duration<double, std::milli>(drawEnd - drawStart).count();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if ((m_opaqueTriangleVertexCount + m_translucentTriangleVertexCount) == 0
        && m_overlayLineVertexCount == 0
        && m_sceneLineVertexCount == 0) {
        painter.setPen(QColor(90, 102, 116));
        painter.drawText(rect(), Qt::AlignCenter, "CAD viewport has no scene geometry");
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

    painter.setPen(hudTextColor());
    painter.drawText(
        QRect(16, 12, width() - 32, 20),
        Qt::AlignLeft | Qt::AlignVCenter,
        QString("Battery CAD viewport  |  GPU-backed  |  Shift-drag move  |  G snap %1  |  Axis %2")
            .arg(m_gridSnapEnabled ? "on" : "off")
            .arg(m_moveAxis == MoveAxis::X ? "X" : m_moveAxis == MoveAxis::Y ? "Y" : m_moveAxis == MoveAxis::Z ? "Z" : "XZ")
    );
    painter.drawText(
        QRect(16, 32, width() - 32, 18),
        Qt::AlignLeft | Qt::AlignVCenter,
        QString("Scene %1 tris  |  Geo %2 ms  |  Packet %3 ms  |  Upload %4 ms  |  GPU draw %5 ms")
            .arg(static_cast<qlonglong>(m_engine.renderDiagnostics().triangle_count))
            .arg(m_engine.renderDiagnostics().last_geometry_build_ms, 0, 'f', 1)
            .arg(m_engine.renderDiagnostics().last_render_packet_ms, 0, 'f', 1)
            .arg(m_lastBufferUploadMs, 0, 'f', 1)
            .arg(m_lastGpuDrawMs, 0, 'f', 1)
    );
    painter.drawText(
        QRect(16, 50, width() - 32, 18),
        Qt::AlignLeft | Qt::AlignVCenter,
        QString("Cell cache %1 hits / %2 misses  |  Rebuilds geo %3 packet %4  |  Depth %5")
            .arg(static_cast<qlonglong>(m_engine.renderDiagnostics().cylindrical_cell_cache_hits))
            .arg(static_cast<qlonglong>(m_engine.renderDiagnostics().cylindrical_cell_cache_misses))
            .arg(static_cast<qlonglong>(m_engine.renderDiagnostics().geometry_rebuild_count))
            .arg(static_cast<qlonglong>(m_engine.renderDiagnostics().packet_rebuild_count))
            .arg(m_depthBits)
    );
}

void CadViewportWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_moveDragging = event->modifiers().testFlag(Qt::ShiftModifier) && m_engine.selectedEntity().isValid();
        m_pressMousePos = event->pos();
        m_lastMousePos = event->pos();
    }
    QOpenGLWidget::mousePressEvent(event);
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
    QOpenGLWidget::mouseMoveEvent(event);
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
    QOpenGLWidget::mouseReleaseEvent(event);
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
    QOpenGLWidget::keyPressEvent(event);
}

void CadViewportWidget::destroyGlResources()
{
    m_sceneFramebuffer.reset();
    if (m_opaqueTriangleBuffer.isCreated()) {
        m_opaqueTriangleBuffer.destroy();
    }
    if (m_translucentTriangleBuffer.isCreated()) {
        m_translucentTriangleBuffer.destroy();
    }
    if (m_sceneLineBuffer.isCreated()) {
        m_sceneLineBuffer.destroy();
    }
    if (m_overlayLineBuffer.isCreated()) {
        m_overlayLineBuffer.destroy();
    }
    if (m_selectionLineBuffer.isCreated()) {
        m_selectionLineBuffer.destroy();
    }
    if (m_vao.isCreated()) {
        m_vao.destroy();
    }
    m_triangleProgram.reset();
    m_lineProgram.reset();
}

void CadViewportWidget::ensureGpuResources()
{
    if (m_triangleProgram != nullptr && m_lineProgram != nullptr) {
        return;
    }

    if (!m_vao.isCreated()) {
        m_vao.create();
    }

    if (!m_opaqueTriangleBuffer.isCreated()) {
        m_opaqueTriangleBuffer.create();
    }
    if (!m_translucentTriangleBuffer.isCreated()) {
        m_translucentTriangleBuffer.create();
    }
    if (!m_sceneLineBuffer.isCreated()) {
        m_sceneLineBuffer.create();
    }
    if (!m_overlayLineBuffer.isCreated()) {
        m_overlayLineBuffer.create();
    }
    if (!m_selectionLineBuffer.isCreated()) {
        m_selectionLineBuffer.create();
    }

    m_triangleProgram = std::make_unique<QOpenGLShaderProgram>();
    m_triangleProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kTriangleVertexShader);
    m_triangleProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kTriangleFragmentShader);
    m_triangleProgram->link();

    m_lineProgram = std::make_unique<QOpenGLShaderProgram>();
    m_lineProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, kLineVertexShader);
    m_lineProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, kLineFragmentShader);
    m_lineProgram->link();
}

void CadViewportWidget::ensureSceneFramebuffer()
{
    const QSize desiredSize(
        std::max(1, static_cast<int>(std::round(width() * devicePixelRatioF()))),
        std::max(1, static_cast<int>(std::round(height() * devicePixelRatioF())))
    );

    if (m_sceneFramebuffer != nullptr && m_sceneFramebuffer->size() == desiredSize) {
        return;
    }

    QOpenGLFramebufferObjectFormat framebufferFormat;
    framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    framebufferFormat.setInternalTextureFormat(GL_RGBA8);
    framebufferFormat.setMipmap(false);
    framebufferFormat.setSamples(0);

    m_sceneFramebuffer = std::make_unique<QOpenGLFramebufferObject>(desiredSize, framebufferFormat);
    if (m_sceneFramebuffer != nullptr && m_sceneFramebuffer->isValid()) {
        m_sceneFramebuffer->bind();
        GLint depthBits = 0;
        glGetIntegerv(GL_DEPTH_BITS, &depthBits);
        m_depthBits = static_cast<int>(depthBits);
        m_sceneFramebuffer->release();
    } else {
        m_sceneFramebuffer.reset();
        m_depthBits = 0;
    }
}

void CadViewportWidget::syncGpuBuffers()
{
    const auto start = std::chrono::steady_clock::now();
    bool uploaded = false;

    if (m_uploadedGeometryRevision != m_engine.renderDiagnostics().geometry_rebuild_count) {
        uploadOpaqueSceneGeometry();
        m_uploadedGeometryRevision = m_engine.renderDiagnostics().geometry_rebuild_count;
        uploaded = true;
    }

    const bool packetRevisionChanged = m_uploadedPacketRevision != m_engine.renderDiagnostics().packet_rebuild_count;
    if (packetRevisionChanged) {
        uploadDynamicLines();
        m_uploadedPacketRevision = m_engine.renderDiagnostics().packet_rebuild_count;
        uploaded = true;
    }

    if (uploaded || packetRevisionChanged) {
        uploadTranslucentSceneGeometry();
    }

    if (uploaded) {
        const auto end = std::chrono::steady_clock::now();
        m_lastBufferUploadMs = std::chrono::duration<double, std::milli>(end - start).count();
    } else {
        m_lastBufferUploadMs = 0.0;
    }
}

void CadViewportWidget::uploadOpaqueSceneGeometry()
{
    if (m_depthBits <= 0) {
        m_opaqueTriangleVertexCount = 0;
    } else {
    const std::vector<GpuTriangleVertex> triangleVertices = buildTriangleVertices(m_engine.visualGeometry());
    m_opaqueTriangleVertexCount = static_cast<int>(triangleVertices.size());
    m_opaqueTriangleBuffer.bind();
    m_opaqueTriangleBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_opaqueTriangleBuffer.allocate(
        triangleVertices.empty() ? nullptr : triangleVertices.data(),
        static_cast<int>(triangleVertices.size() * sizeof(GpuTriangleVertex))
    );
    m_opaqueTriangleBuffer.release();
    }

    const std::vector<GpuLineVertex> sceneLineVertices = buildSceneLineVertices(m_engine.visualGeometry());
    m_sceneLineVertexCount = static_cast<int>(sceneLineVertices.size());
    m_sceneLineBuffer.bind();
    m_sceneLineBuffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_sceneLineBuffer.allocate(
        sceneLineVertices.empty() ? nullptr : sceneLineVertices.data(),
        static_cast<int>(sceneLineVertices.size() * sizeof(GpuLineVertex))
    );
    m_sceneLineBuffer.release();
}

void CadViewportWidget::uploadTranslucentSceneGeometry()
{
    const std::vector<GpuTriangleVertex> triangleVertices = m_depthBits <= 0
        ? buildSortedTriangleVertices(m_engine.visualGeometry(), m_engine.renderPacket().mvp)
        : buildTransparentTriangleVertices(m_engine.visualGeometry(), m_engine.renderPacket().mvp);
    m_translucentTriangleVertexCount = static_cast<int>(triangleVertices.size());
    m_translucentTriangleBuffer.bind();
    m_translucentTriangleBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_translucentTriangleBuffer.allocate(
        triangleVertices.empty() ? nullptr : triangleVertices.data(),
        static_cast<int>(triangleVertices.size() * sizeof(GpuTriangleVertex))
    );
    m_translucentTriangleBuffer.release();
}

void CadViewportWidget::uploadDynamicLines()
{
    const cad::render::RenderPacket& frame = m_engine.renderPacket();

    const std::vector<GpuLineVertex> overlayLineVertices = buildPacketLineVertices(frame.lines);
    m_overlayLineVertexCount = static_cast<int>(overlayLineVertices.size());
    m_overlayLineBuffer.bind();
    m_overlayLineBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_overlayLineBuffer.allocate(
        overlayLineVertices.empty() ? nullptr : overlayLineVertices.data(),
        static_cast<int>(overlayLineVertices.size() * sizeof(GpuLineVertex))
    );
    m_overlayLineBuffer.release();

    const std::vector<GpuLineVertex> selectionLineVertices = buildPacketLineVertices(frame.selection_overlay_lines, 0.98f);
    m_selectionLineVertexCount = static_cast<int>(selectionLineVertices.size());
    m_selectionLineBuffer.bind();
    m_selectionLineBuffer.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_selectionLineBuffer.allocate(
        selectionLineVertices.empty() ? nullptr : selectionLineVertices.data(),
        static_cast<int>(selectionLineVertices.size() * sizeof(GpuLineVertex))
    );
    m_selectionLineBuffer.release();
}
