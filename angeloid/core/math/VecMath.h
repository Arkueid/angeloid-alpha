#pragma once

#include <algorithm>
#include <array>
#include <cmath>

struct Vec2 {
    float x = 0, y = 0;
};
struct Vec3 {
    float x = 0, y = 0, z = 0;
};
struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
};
struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
};

inline std::array<float, 4> quatSlerp(const std::array<float, 4>& qa,
                                      const std::array<float, 4>& qb, float t) {
    if (t <= 0)
        return qa;
    if (t >= 1)
        return qb;
    std::array<float, 4> a = qa, b = qb;
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0) {
        b[0] = -b[0];
        b[1] = -b[1];
        b[2] = -b[2];
        b[3] = -b[3];
        dot = -dot;
    }
    if (dot > 0.9995f) {
        std::array<float, 4> r = {a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]),
                                  a[2] + t * (b[2] - a[2]), a[3] + t * (b[3] - a[3])};
        float len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
        if (len > 0) {
            r[0] /= len;
            r[1] /= len;
            r[2] /= len;
            r[3] /= len;
        }
        return r;
    }
    float theta0 = std::acos(std::max(-1.0f, std::min(1.0f, dot)));
    float theta = theta0 * t;
    float st = std::sin(theta), st0 = std::sin(theta0);
    float s1 = std::cos(theta) - dot * st / st0;
    float s2 = st / st0;
    return {s1 * a[0] + s2 * b[0], s1 * a[1] + s2 * b[1], s1 * a[2] + s2 * b[2],
            s1 * a[3] + s2 * b[3]};
}

// --- Vec3 helpers ---

inline float vec3Length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vec3 vec3Normalize(const Vec3& v) {
    float len = vec3Length(v);
    if (len < 1e-12f)
        return {0, 0, 0};
    return {v.x / len, v.y / len, v.z / len};
}

inline float vec3Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 vec3Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Vec3 vec3Sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3Add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 vec3Mul(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

inline Vec3 vec3Neg(const Vec3& v) {
    return {-v.x, -v.y, -v.z};
}

// --- Quat helpers ---

inline Quat quatNormalize(const Quat& q) {
    float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len < 1e-12f)
        return {0, 0, 0, 1};
    return {q.x / len, q.y / len, q.z / len, q.w / len};
}

inline Quat quatConjugate(const Quat& q) {
    return {-q.x, -q.y, -q.z, q.w};
}

inline Quat quatFromAxisAngle(const Vec3& axis, float angleRad) {
    float half = angleRad * 0.5f;
    float s = std::sin(half);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
}

// Shortest-arc quaternion from 'from' to 'to' (both must be normalized).
// Degenerate cases handled: parallel returns identity, opposite returns 180° about perpendicular axis.
inline Quat quatFromTwoVectors(const Vec3& from, const Vec3& to) {
    Vec3 f = vec3Normalize(from);
    Vec3 t = vec3Normalize(to);
    float d = vec3Dot(f, t);
    if (d > 0.9999f)
        return {0, 0, 0, 1};
    if (d < -0.9999f) {
        Vec3 axis = vec3Cross(f, {1, 0, 0});
        if (vec3Length(axis) < 1e-6f)
            axis = vec3Cross(f, {0, 1, 0});
        return quatFromAxisAngle(vec3Normalize(axis), 3.14159265f);
    }
    Vec3 axis = vec3Cross(f, t);
    float w = d + 1.0f;  // w = d + 1 avoids acos; equivalent to shortest arc
    float len = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z + w * w);
    return {axis.x / len, axis.y / len, axis.z / len, w / len};
}

inline std::array<float, 4> quatToArray(const Quat& q) {
    return {q.x, q.y, q.z, q.w};
}

inline Quat quatFromArray(const std::array<float, 4>& a) {
    return {a[0], a[1], a[2], a[3]};
}

inline Quat quatMul(const Quat& a, const Quat& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// --- Column-major 4×4 matrix helpers ---

using Mat4 = std::array<float, 16>;

inline Mat4 mat4Identity() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

inline Mat4 mat4Mul(const Mat4& A, const Mat4& B) {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0;
            for (int k = 0; k < 4; ++k)
                sum += A[k * 4 + row] * B[col * 4 + k];
            r[col * 4 + row] = sum;
        }
    }
    return r;
}

inline Mat4 mat4InverseAffine(const Mat4& M) {
    Mat4 r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r[i * 4 + j] = M[j * 4 + i];
    r[3 * 4 + 3] = 1.0f;
    float tx = M[12], ty = M[13], tz = M[14];
    r[12] = -(r[0] * tx + r[4] * ty + r[8] * tz);
    r[13] = -(r[1] * tx + r[5] * ty + r[9] * tz);
    r[14] = -(r[2] * tx + r[6] * ty + r[10] * tz);
    return r;
}

// Convert Quat to column-major 4×4 rotation matrix (Hamilton convention, matching
// VpdPose::toMatrix() and quatFromTwoVectors).
inline Mat4 mat4FromQuat(const Quat& q) {
    float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
    float xx = q.x * x2, xy = q.x * y2, xz = q.x * z2;
    float yy = q.y * y2, yz = q.y * z2, zz = q.z * z2;
    float wx = q.w * x2, wy = q.w * y2, wz = q.w * z2;
    // Hamilton: v' = q * v * q_conj
    return {1.0f - (yy + zz), xy + wz, xz - wy, 0,
            xy - wz,          1.0f - (xx + zz), yz + wx, 0,
            xz + wy,          yz - wx, 1.0f - (xx + yy), 0,
            0,                0,       0,                 1.0f};
}

// Transform a point (Vec3) by a column-major 4x4 matrix: result = M * (v, 1)
inline Vec3 mat4TransformPoint(const Mat4& M, const Vec3& v) {
    float x = M[0] * v.x + M[4] * v.y + M[8] * v.z + M[12];
    float y = M[1] * v.x + M[5] * v.y + M[9] * v.z + M[13];
    float z = M[2] * v.x + M[6] * v.y + M[10] * v.z + M[14];
    return {x, y, z};
}

// Transform a direction (Vec3) by a column-major 4x4 matrix: result = M * (v, 0)
inline Vec3 mat4TransformDir(const Mat4& M, const Vec3& v) {
    float x = M[0] * v.x + M[4] * v.y + M[8] * v.z;
    float y = M[1] * v.x + M[5] * v.y + M[9] * v.z;
    float z = M[2] * v.x + M[6] * v.y + M[10] * v.z;
    return {x, y, z};
}

// Extract translation from column-major 4x4 matrix
inline Vec3 mat4Translation(const Mat4& M) {
    return {M[12], M[13], M[14]};
}
