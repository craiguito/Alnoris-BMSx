#include "Camera.h"

#include <cmath>

namespace cad::camera {

void Camera::orbit(float delta_yaw_deg, float delta_pitch_deg)
{
    m_yawDeg += delta_yaw_deg;
    m_pitchDeg = math::clamp(m_pitchDeg + delta_pitch_deg, -80.0f, -4.0f);
}

void Camera::zoom(float delta)
{
    m_zoom = math::clamp(m_zoom + delta, 0.45f, 2.3f);
}

void Camera::fitToBounds(const math::Vec3& center, const math::Vec3& size)
{
    m_target = center;
    const float half_x = std::max(10.0f, size.x * 0.5f);
    const float half_y = std::max(10.0f, size.y * 0.5f);
    const float half_z = std::max(10.0f, size.z * 0.5f);
    const float radius = std::sqrt(half_x * half_x + half_y * half_y + half_z * half_z);
    const bool elongated_x = size.x > size.z * 1.8f;
    if (elongated_x) {
        m_target.y += size.y * 0.16f;
    }
    m_distance = std::max(180.0f, radius * (elongated_x ? 1.58f : 2.55f));
    m_zoom = elongated_x ? 1.08f : 1.0f;
    m_yawDeg = elongated_x ? -40.0f : -32.0f;
    m_pitchDeg = elongated_x ? -38.0f : -18.0f;
}

math::Mat4 Camera::viewMatrix() const
{
    const math::Mat4 cameraDistance = math::translation(0.0f, 0.0f, -m_distance / m_zoom);
    const math::Mat4 pitch = math::rotation_x(m_pitchDeg);
    const math::Mat4 yaw = math::rotation_y(m_yawDeg);
    const math::Mat4 target = math::translation(-m_target.x, -m_target.y, -m_target.z);
    return math::multiply(cameraDistance, math::multiply(pitch, math::multiply(yaw, target)));
}

math::Mat4 Camera::projectionMatrix(float aspect_ratio) const
{
    return math::perspective(42.0f, aspect_ratio, 1.0f, 5000.0f);
}

} // namespace cad::camera
