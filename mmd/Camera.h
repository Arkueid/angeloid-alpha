#pragma once

#include <array>
#include <cmath>

class Camera {
   public:
    static Camera& instance()
    {
        static Camera cam;
        return cam;
    }

    void reset();

    // Move camera based on key states (caller extracts keys from window system)
    void update(float deltaTime, bool w, bool a, bool s, bool d, bool e, bool q);

    void onMouseButton(bool pressed);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double yoffset);

    // Column-major 4x4 matrices
    std::array<float, 16> viewMatrix() const;
    static std::array<float, 16> projectionMatrix(int width, int height, float fov = 45.0f,
                                                  float nearPlane = 0.1f, float farPlane = 500.0f);

    float x = 0, y = 0, z = 10;
    float rotX = 0, rotY = 0;  // degrees
    float speed = 5.0f;
    float mouseSensitivity = 0.1f;

   private:
    Camera() = default;
    bool mPanning = false;
    double mLastMouseX = 0, mLastMouseY = 0;
};
