#include "Camera.h"

namespace cad::camera {

void Camera::orbit(float delta_yaw_deg, float delta_pitch_deg)
{
    m_yawDeg += delta_yaw_deg;
    m_pitchDeg = math::clamp(m_pitchDeg + delta_pitch_deg, -80.0f, 15.0f);
}

void Camera::zoom(float delta)
{
    m_zoom = math::clamp(m_zoom + delta, 0.45f, 2.3f);
}

math::Mat4 Camera::viewMatrix() const
{
    return math::multiply(
        math::multiply(math::translation(0.0f, -80.0f, -1000.0f / m_zoom), math::rotation_x(m_pitchDeg)),
        math::rotation_y(m_yawDeg)
    );
}

math::Mat4 Camera::projectionMatrix(float aspect_ratio) const
{
    return math::perspective(42.0f, aspect_ratio, 1.0f, 5000.0f);
}

} // namespace cad::camera
