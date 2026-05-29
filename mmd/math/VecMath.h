#pragma once

#include <algorithm>
#include <array>
#include <cmath>

struct Vec2 { float x = 0, y = 0; };
struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec4 { float x = 0, y = 0, z = 0, w = 0; };
struct Quat { float x = 0, y = 0, z = 0, w = 1; };

inline std::array<float, 4> quatSlerp(const std::array<float, 4>& qa,
                                       const std::array<float, 4>& qb, float t)
{
    if (t <= 0) return qa;
    if (t >= 1) return qb;
    std::array<float, 4> a = qa, b = qb;
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    if (dot < 0) { b[0] = -b[0]; b[1] = -b[1]; b[2] = -b[2]; b[3] = -b[3]; dot = -dot; }
    if (dot > 0.9995f) {
        std::array<float, 4> r = {a[0]+t*(b[0]-a[0]), a[1]+t*(b[1]-a[1]),
                                   a[2]+t*(b[2]-a[2]), a[3]+t*(b[3]-a[3])};
        float len = std::sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2] + r[3]*r[3]);
        if (len > 0) { r[0]/=len; r[1]/=len; r[2]/=len; r[3]/=len; }
        return r;
    }
    float theta0 = std::acos(std::max(-1.0f, std::min(1.0f, dot)));
    float theta = theta0 * t;
    float st = std::sin(theta), st0 = std::sin(theta0);
    float s1 = std::cos(theta) - dot * st / st0;
    float s2 = st / st0;
    return {s1*a[0]+s2*b[0], s1*a[1]+s2*b[1], s1*a[2]+s2*b[2], s1*a[3]+s2*b[3]};
}
