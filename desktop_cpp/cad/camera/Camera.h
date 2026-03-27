#pragma once

#include "../math/CadMath.h"

namespace cad::camera {

class Camera
{
public:
    void orbit(float delta_yaw_deg, float delta_pitch_deg);
    void zoom(float delta);

    [[nodiscard]] math::Mat4 viewMatrix() const;
    [[nodiscard]] math::Mat4 projectionMatrix(float aspect_ratio) const;

private:
    float m_yawDeg = -32.0f;
    float m_pitchDeg = -18.0f;
    float m_zoom = 1.0f;
};

} // namespace cad::camera
