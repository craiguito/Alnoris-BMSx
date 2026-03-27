#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace cad::math {

constexpr float kPi = 3.14159265358979323846f;

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Mat4
{
    std::array<float, 16> m{};
};

inline float clamp(float value, float low, float high)
{
    return std::max(low, std::min(high, value));
}

inline Vec3 add(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 sub(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 mul(const Vec3& a, float scalar)
{
    return {a.x * scalar, a.y * scalar, a.z * scalar};
}

inline Vec3 mix(const Vec3& a, const Vec3& b, float amount)
{
    const float t = clamp(amount, 0.0f, 1.0f);
    return add(a, mul(sub(b, a), t));
}

inline Mat4 identity()
{
    Mat4 out{};
    out.m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return out;
}

inline Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 out{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            float value = 0.0f;
            for (int k = 0; k < 4; ++k) {
                value += a.m[(row * 4) + k] * b.m[(k * 4) + col];
            }
            out.m[(row * 4) + col] = value;
        }
    }
    return out;
}

inline Vec4 multiply(const Mat4& m, const Vec4& v)
{
    return {
        (m.m[0] * v.x) + (m.m[1] * v.y) + (m.m[2] * v.z) + (m.m[3] * v.w),
        (m.m[4] * v.x) + (m.m[5] * v.y) + (m.m[6] * v.z) + (m.m[7] * v.w),
        (m.m[8] * v.x) + (m.m[9] * v.y) + (m.m[10] * v.z) + (m.m[11] * v.w),
        (m.m[12] * v.x) + (m.m[13] * v.y) + (m.m[14] * v.z) + (m.m[15] * v.w)
    };
}

inline Mat4 perspective(float fov_deg, float aspect, float near_plane, float far_plane)
{
    const float f = 1.0f / std::tan((fov_deg * kPi / 180.0f) * 0.5f);
    Mat4 out{};
    out.m = {
        f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, (far_plane + near_plane) / (near_plane - far_plane), (2.0f * far_plane * near_plane) / (near_plane - far_plane),
        0.0f, 0.0f, -1.0f, 0.0f
    };
    return out;
}

inline Mat4 translation(float x, float y, float z)
{
    Mat4 out = identity();
    out.m[3] = x;
    out.m[7] = y;
    out.m[11] = z;
    return out;
}

inline Mat4 rotation_x(float deg)
{
    const float r = deg * kPi / 180.0f;
    const float c = std::cos(r);
    const float s = std::sin(r);
    Mat4 out = identity();
    out.m[5] = c;
    out.m[6] = -s;
    out.m[9] = s;
    out.m[10] = c;
    return out;
}

inline Mat4 rotation_y(float deg)
{
    const float r = deg * kPi / 180.0f;
    const float c = std::cos(r);
    const float s = std::sin(r);
    Mat4 out = identity();
    out.m[0] = c;
    out.m[2] = s;
    out.m[8] = -s;
    out.m[10] = c;
    return out;
}

} // namespace cad::math
