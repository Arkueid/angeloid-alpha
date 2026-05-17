#pragma once

#include <array>
#include <cmath>

struct GLFWwindow;

class Camera {
public:
    Camera();

    void reset();
    void update(GLFWwindow* window, float deltaTime);

    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);

    // Returns 16 floats in column-major order
    std::array<float, 16> viewMatrix() const;
    static std::array<float, 16> projectionMatrix(int width, int height,
                                                   float fov = 45.0f,
                                                   float nearPlane = 0.1f,
                                                   float farPlane = 500.0f);

    float x = 0, y = 0, z = 10;
    float rotX = 0, rotY = 0;    // degrees
    float speed = 5.0f;
    float mouseSensitivity = 0.1f;

    bool isPanning = false;
    double lastMouseX = 0, lastMouseY = 0;
};
