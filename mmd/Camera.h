#pragma once

#include <array>
#include <cmath>

enum class CameraMode { FPS = 0, Orbit = 1 };

class Camera {
public:
    static Camera& instance() {
        static Camera cam;
        return cam;
    }

    // Mouse button constants (match GLFW values so callers can pass directly)
    static constexpr int MOUSE_BUTTON_LEFT   = 0;
    static constexpr int MOUSE_BUTTON_RIGHT  = 1;
    static constexpr int MOUSE_BUTTON_MIDDLE = 2;
    static constexpr int MOUSE_PRESS   = 1;
    static constexpr int MOUSE_RELEASE = 0;
    static constexpr int MOD_SHIFT = 0x0001;

    void reset();

    CameraMode mode() const { return mMode; }
    void setMode(CameraMode mode);
    void toggleMode();

    // Current camera eye position in world space (works in both modes)
    void getEyePosition(float& ex, float& ey, float& ez) const;

    // Move camera based on key states (caller extracts keys from window system)
    void update(float deltaTime, bool w, bool a, bool s, bool d, bool e, bool q);

    // Single-button convenience (backward compat) — delegates to multi-button version
    void onMouseButton(bool pressed);
    // Multi-button with modifier
    void onMouseButton(int button, int action, int mods);
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

    // --- Orbit mode state ---
    float targetX = 0, targetY = 0, targetZ = 0;
    float orbitDistance = 10.0f;
    float orbitLatitude = 90.0f;    // degrees; 90=horizon, 0=top, 180=bottom
    float orbitLongitude = 180.0f;  // degrees; 0=-Z, 180=+Z (matches FPS default view)
    float orbitFov = 45.0f;
    float orbitSensitivity = 0.005f; // radians per pixel
    float dollySpeed = 0.05f;

private:
    Camera() = default;
    CameraMode mMode = CameraMode::FPS;
    enum class OrbitAction { None, Orbit, Pan };
    OrbitAction mOrbitAction = OrbitAction::None;
    bool mPanning = false;
    double mLastMouseX = 0, mLastMouseY = 0;
};
