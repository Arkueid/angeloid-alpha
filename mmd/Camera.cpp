#include "Camera.h"
#include "math/VecMath.h"

#include <algorithm>
#include <cmath>

void Camera::reset() {
    if (mMode == CameraMode::FPS) {
        x = 0;
        y = 0;
        z = 10;
        rotX = 0;
        rotY = 0;
        speed = 5.0f;
    } else {
        targetX = 0;
        targetY = 0;
        targetZ = 0;
        orbitDistance = 10.0f;
        orbitLatitude = 90.0f;
        orbitLongitude = 180.0f;
        orbitFov = 45.0f;
    }
}

void Camera::setMode(CameraMode newMode) {
    if (newMode == mMode)
        return;

    if (newMode == CameraMode::Orbit) {
        // FPS -> Orbit: convert FPS position to spherical params around target
        Vec3 target{targetX, targetY, targetZ};
        Vec3 eye{x, y, z};
        Vec3 toEye = vec3Sub(eye, target);
        float dist = vec3Length(toEye);
        if (dist < 0.001f) {
            orbitDistance = 10.0f;
            orbitLatitude = 90.0f;
            orbitLongitude = 180.0f;
        } else {
            Vec3 dir = vec3Mul(toEye, 1.0f / dist);
            orbitDistance = dist;
            // dir = (-st*sp, ct, -st*cp) where st=sin(theta), ct=cos(theta), sp=sin(phi), cp=cos(phi)
            // theta = acos(clamp(dir.y, -1, 1))
            // phi = atan2(-dir.x, -dir.z) [when sp and cp match]
            float theta = std::acos(std::max(-1.0f, std::min(1.0f, dir.y)));
            float phi = std::atan2(-dir.x, -dir.z);
            orbitLatitude = theta * 180.0f / 3.14159265f;
            orbitLongitude = phi * 180.0f / 3.14159265f;
            orbitLatitude = std::max(1.8f, std::min(178.2f, orbitLatitude));
        }
    } else {
        // Orbit -> FPS: set FPS position/orientation to match current orbit view
        float theta = orbitLatitude * 3.14159265f / 180.0f;
        float phi = orbitLongitude * 3.14159265f / 180.0f;
        float st = std::sin(theta), ct = std::cos(theta);
        float sp = std::sin(phi), cp = std::cos(phi);

        x = targetX + (-st * sp) * orbitDistance;
        y = targetY + ct * orbitDistance;
        z = targetZ + (-st * cp) * orbitDistance;

        // Derive FPS yaw/pitch from view direction.
        // FPS view front = (-sin(rotY)*cos(rotX), sin(rotX), -cos(rotY)*cos(rotX))
        // Orbit view dir (eye->target) = (st*sp, -ct, st*cp)
        float rotXRad = std::asin(std::max(-1.0f, std::min(1.0f, -ct)));
        rotX = rotXRad * 180.0f / 3.14159265f;
        rotX = std::max(-89.0f, std::min(89.0f, rotX));
        rotY = std::atan2(st * sp, st * cp) * 180.0f / 3.14159265f;
    }

    mMode = newMode;
    mPanning = false;
    mOrbitAction = OrbitAction::None;
}

void Camera::getEyePosition(float& ex, float& ey, float& ez) const {
    if (mMode == CameraMode::FPS) {
        ex = x; ey = y; ez = z;
    } else {
        float theta = orbitLatitude * 3.14159265f / 180.0f;
        float phi = orbitLongitude * 3.14159265f / 180.0f;
        float st = std::sin(theta), ct = std::cos(theta);
        float sp = std::sin(phi), cp = std::cos(phi);
        // Dir from target to eye: (-st*sp, ct, -st*cp)
        ex = targetX + (-st * sp) * orbitDistance;
        ey = targetY + ct * orbitDistance;
        ez = targetZ + (-st * cp) * orbitDistance;
    }
}

void Camera::toggleMode() {
    setMode(mMode == CameraMode::FPS ? CameraMode::Orbit : CameraMode::FPS);
}

void Camera::update(float deltaTime, bool w, bool a, bool s, bool d, bool e, bool q) {
    if (mMode != CameraMode::FPS)
        return;

    float dt = speed * deltaTime;

    float rotYRad = rotY * 3.14159265f / 180.0f;
    float rotXRad = rotX * 3.14159265f / 180.0f;

    float frontX = -std::sin(rotYRad) * std::cos(rotXRad);
    float frontY = std::sin(rotXRad);
    float frontZ = -std::cos(rotYRad) * std::cos(rotXRad);

    float rightX = std::cos(rotYRad);
    float rightZ = -std::sin(rotYRad);

    if (w) {
        x += frontX * dt;
        y += frontY * dt;
        z += frontZ * dt;
    }
    if (s) {
        x -= frontX * dt;
        y -= frontY * dt;
        z -= frontZ * dt;
    }
    if (a) {
        x -= rightX * dt;
        z -= rightZ * dt;
    }
    if (d) {
        x += rightX * dt;
        z += rightZ * dt;
    }
    if (e) {
        y += dt;
    }
    if (q) {
        y -= dt;
    }
}

void Camera::onMouseButton(bool pressed) {
    onMouseButton(MOUSE_BUTTON_LEFT, pressed ? MOUSE_PRESS : MOUSE_RELEASE, 0);
}

void Camera::onMouseButton(int button, int action, int mods) {
    if (action == MOUSE_PRESS) {
        switch (mMode) {
        case CameraMode::FPS:
            if (button == MOUSE_BUTTON_LEFT)
                mPanning = true;
            break;
        case CameraMode::Orbit:
            if ((button == MOUSE_BUTTON_LEFT && (mods & MOD_SHIFT)) ||
                 button == MOUSE_BUTTON_MIDDLE) {
                mOrbitAction = OrbitAction::Pan;
            } else if (button == MOUSE_BUTTON_LEFT) {
                mOrbitAction = OrbitAction::Orbit;
            }
            break;
        }
    } else {
        mPanning = false;
        mOrbitAction = OrbitAction::None;
    }
}

void Camera::onCursorPos(double xpos, double ypos) {
    double dx = xpos - mLastMouseX;
    double dy = ypos - mLastMouseY;
    mLastMouseX = xpos;
    mLastMouseY = ypos;

    switch (mMode) {
    case CameraMode::FPS:
        if (!mPanning)
            return;
        rotY -= (float)(dx * mouseSensitivity);
        rotX -= (float)(dy * mouseSensitivity);
        rotX = std::max(-89.0f, std::min(89.0f, rotX));
        break;

    case CameraMode::Orbit:
        if (mOrbitAction == OrbitAction::Orbit) {
            orbitLongitude -= (float)(dx * orbitSensitivity * 180.0f);
            orbitLatitude -= (float)(dy * orbitSensitivity * 180.0f);
            orbitLatitude = std::max(1.8f, std::min(178.2f, orbitLatitude));
            if (orbitLongitude >= 360.0f)
                orbitLongitude -= 360.0f;
            if (orbitLongitude < 0.0f)
                orbitLongitude += 360.0f;
        } else if (mOrbitAction == OrbitAction::Pan) {
            float theta = orbitLatitude * 3.14159265f / 180.0f;
            float phi = orbitLongitude * 3.14159265f / 180.0f;
            float st = std::sin(theta), ct = std::cos(theta);
            float sp = std::sin(phi), cp = std::cos(phi);

            // Direction from eye to target
            Vec3 f{st * sp, -ct, st * cp};
            f = vec3Normalize(f);
            Vec3 worldUp{0, 1, 0};
            Vec3 s = vec3Normalize(vec3Cross(f, worldUp));
            Vec3 u = vec3Cross(s, f);

            float panScale = orbitDistance * 0.002f;
            float mx = (float)(-dx * panScale);
            float my = (float)(dy * panScale);
            targetX += s.x * mx + u.x * my;
            targetY += s.y * mx + u.y * my;
            targetZ += s.z * mx + u.z * my;
        }
        break;
    }
}

void Camera::onScroll(double yoffset) {
    if (mMode == CameraMode::FPS) {
        if (yoffset > 0) {
            if (speed >= 0.1f)
                speed += 0.1f;
            else if (speed >= 0.01f)
                speed += 0.01f;
            else
                speed += 0.001f;
        } else {
            if (speed > 0.1f)
                speed -= 0.1f;
            else if (speed > 0.01f)
                speed -= 0.01f;
            else
                speed -= 0.001f;
        }
        speed = std::max(0.001f, std::min(20.0f, speed));
    } else {
        orbitDistance *= (float)(1.0 - yoffset * dollySpeed);
        orbitDistance = std::max(0.1f, std::min(500.0f, orbitDistance));
    }
}

std::array<float, 16> Camera::viewMatrix() const {
    if (mMode == CameraMode::FPS) {
        float cx = std::cos(rotX * 3.14159265f / 180.0f);
        float cy = std::cos(rotY * 3.14159265f / 180.0f);
        float sx = std::sin(rotX * 3.14159265f / 180.0f);
        float sy = std::sin(rotY * 3.14159265f / 180.0f);

        return {cy,
                sx * sy,
                cx * sy,
                0,
                0,
                cx,
                -sx,
                0,
                -sy,
                sx * cy,
                cx * cy,
                0,
                -cy * x + sy * z,
                -(cx * y + sx * sy * x + sx * cy * z),
                sx * y - cx * sy * x - cx * cy * z,
                1.0f};
    }

    // Orbit mode: right-handed lookAt
    float theta = orbitLatitude * 3.14159265f / 180.0f;
    float phi = orbitLongitude * 3.14159265f / 180.0f;
    float st = std::sin(theta), ct = std::cos(theta);
    float sp = std::sin(phi), cp = std::cos(phi);

    // Dir from target to eye: (-st*sp, ct, -st*cp)
    Vec3 eye{targetX + (-st * sp) * orbitDistance,
             targetY + ct * orbitDistance,
             targetZ + (-st * cp) * orbitDistance};
    Vec3 target{targetX, targetY, targetZ};

    // Standard right-handed lookAt
    Vec3 f = vec3Normalize(vec3Sub(target, eye));   // forward (eye → target)
    Vec3 worldUp{0, 1, 0};
    Vec3 s = vec3Normalize(vec3Cross(f, worldUp));  // right
    Vec3 u = vec3Cross(s, f);                       // camUp

    return {s.x, u.x, -f.x, 0,
            s.y, u.y, -f.y, 0,
            s.z, u.z, -f.z, 0,
            -vec3Dot(s, eye), -vec3Dot(u, eye), vec3Dot(f, eye), 1.0f};
}

std::array<float, 16> Camera::projectionMatrix(int width, int height, float fov, float nearPlane,
                                               float farPlane) {
    float aspect = (float)width / (float)height;
    float f = 1.0f / std::tan(fov * 3.14159265f / 360.0f);
    float a = -(farPlane + nearPlane) / (farPlane - nearPlane);
    float b = -2.0f * farPlane * nearPlane / (farPlane - nearPlane);

    return {f / aspect, 0, 0, 0, 0, f, 0, 0, 0, 0, a, -1, 0, 0, b, 0};
}
