#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <functional>
#include <string>

class Application {
public:
    Application(int width, int height, const std::string& title);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

    GLFWwindow* window() const { return mWindow; }
    float deltaTime() const { return mDeltaTime; }
    int width() const { return mWidth; }
    int height() const { return mHeight; }

    // Override these in subclass or set callbacks
    std::function<void(float)> onUpdate;
    std::function<void()> onRender;
    std::function<void(int, int)> onResize;
    std::function<void(int, int, int, int)> onKey;
    std::function<void(int, int, int)> onMouseButton;
    std::function<void(double, double)> onCursorPos;
    std::function<void(double, double)> onScroll;

    void setTitle(const std::string& title);

private:
    static void framebufferSizeCallback(GLFWwindow* win, int w, int h);
    static void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* win, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* win, double x, double y);
    static void scrollCallback(GLFWwindow* win, double xoffset, double yoffset);

    GLFWwindow* mWindow = nullptr;
    int mWidth;
    int mHeight;
    float mLastTime = 0.0f;
    float mDeltaTime = 0.0f;
};
