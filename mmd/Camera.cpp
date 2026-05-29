#include "Camera.h"

#include <algorithm>
#include <cmath>

void Camera::reset()
{
    x = 0; y = 0; z = 10;
    rotX = 0; rotY = 0;
}

void Camera::update(float deltaTime, bool w, bool a, bool s, bool d, bool e, bool q)
{
    float dt = speed * deltaTime;

    float rotYRad = rotY * 3.14159265f / 180.0f;
    float rotXRad = rotX * 3.14159265f / 180.0f;

    float frontX = -std::sin(rotYRad) * std::cos(rotXRad);
    float frontY = std::sin(rotXRad);
    float frontZ = -std::cos(rotYRad) * std::cos(rotXRad);

    float rightX = std::cos(rotYRad);
    float rightZ = -std::sin(rotYRad);

    if (w) { x += frontX * dt; y += frontY * dt; z += frontZ * dt; }
    if (s) { x -= frontX * dt; y -= frontY * dt; z -= frontZ * dt; }
    if (a) { x -= rightX * dt; z -= rightZ * dt; }
    if (d) { x += rightX * dt; z += rightZ * dt; }
    if (e) { y += dt; }
    if (q) { y -= dt; }
}

void Camera::onMouseButton(bool pressed)
{
    mPanning = pressed;
}

void Camera::onCursorPos(double xpos, double ypos)
{
    if (!mPanning) { mLastMouseX = xpos; mLastMouseY = ypos; return; }

    double dx = xpos - mLastMouseX;
    double dy = ypos - mLastMouseY;

    rotY -= (float)(dx * mouseSensitivity);
    rotX -= (float)(dy * mouseSensitivity);
    rotX = std::max(-89.0f, std::min(89.0f, rotX));

    mLastMouseX = xpos;
    mLastMouseY = ypos;
}

void Camera::onScroll(double yoffset)
{
    if (yoffset > 0) {
        if (speed >= 0.1f)       speed += 0.1f;
        else if (speed >= 0.01f) speed += 0.01f;
        else                     speed += 0.001f;
    } else {
        if (speed > 0.1f)        speed -= 0.1f;
        else if (speed > 0.01f)  speed -= 0.01f;
        else                     speed -= 0.001f;
    }
    speed = std::max(0.001f, std::min(20.0f, speed));
}

std::array<float, 16> Camera::viewMatrix() const
{
    float cx = std::cos(rotX * 3.14159265f / 180.0f);
    float cy = std::cos(rotY * 3.14159265f / 180.0f);
    float sx = std::sin(rotX * 3.14159265f / 180.0f);
    float sy = std::sin(rotY * 3.14159265f / 180.0f);

    return {
        cy,       sx * sy,  cx * sy,  0,
        0,        cx,      -sx,        0,
       -sy,       sx * cy,  cx * cy,  0,
       -cy * x + sy * z,
       -(cx * y + sx * sy * x + sx * cy * z),
        sx * y - cx * sy * x - cx * cy * z,
        1.0f
    };
}

std::array<float, 16> Camera::projectionMatrix(int width, int height,
                                                float fov, float nearPlane, float farPlane)
{
    float aspect = (float)width / (float)height;
    float f = 1.0f / std::tan(fov * 3.14159265f / 360.0f);
    float a = -(farPlane + nearPlane) / (farPlane - nearPlane);
    float b = -2.0f * farPlane * nearPlane / (farPlane - nearPlane);

    return {
        f / aspect, 0,  0,  0,
        0,          f,  0,  0,
        0,          0,  a, -1,
        0,          0,  b,  0
    };
}
