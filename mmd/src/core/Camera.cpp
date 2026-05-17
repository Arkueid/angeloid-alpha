#include "core/Camera.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

Camera::Camera() = default;

void Camera::reset()
{
    x = 0; y = 0; z = 10;
    rotX = 0; rotY = 0;
}

void Camera::update(GLFWwindow* window, float deltaTime)
{
    float s = speed * deltaTime;

    float rotYRad = rotY * 3.14159265f / 180.0f;
    float rotXRad = rotX * 3.14159265f / 180.0f;

    float frontX = -std::sin(rotYRad) * std::cos(rotXRad);
    float frontY = std::sin(rotXRad);
    float frontZ = -std::cos(rotYRad) * std::cos(rotXRad);

    float rightX = std::cos(rotYRad);
    float rightZ = -std::sin(rotYRad);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        x += frontX * s; y += frontY * s; z += frontZ * s;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        x -= frontX * s; y -= frontY * s; z -= frontZ * s;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        x -= rightX * s; z -= rightZ * s;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        x += rightX * s; z += rightZ * s;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        y += s;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        y -= s;
    }
}

void Camera::onMouseButton(int button, int action, int mods)
{
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        isPanning = (action == GLFW_PRESS);
    }
}

void Camera::onCursorPos(double xpos, double ypos)
{
    if (!isPanning) {
        lastMouseX = xpos;
        lastMouseY = ypos;
        return;
    }

    double dx = xpos - lastMouseX;
    double dy = ypos - lastMouseY;

    rotY -= (float)(dx * mouseSensitivity);
    rotX -= (float)(dy * mouseSensitivity);

    rotX = std::max(-89.0f, std::min(89.0f, rotX));

    lastMouseX = xpos;
    lastMouseY = ypos;
}

void Camera::onScroll(double xoffset, double yoffset)
{
    (void)xoffset;
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

    // V = R_x(rotX) * R_y(rotY) * T(-pos) in row-major.
    // Return V^T in column-major for GL.
    // Derived analytically and verified against Python at rotX,rotY ∈ {0,30,45}°.
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
