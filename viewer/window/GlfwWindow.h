#pragma once

#include "window/IWindow.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

class GlfwWindow : public IWindow {
public:
    GlfwWindow(int width, int height, const std::string& title);
    ~GlfwWindow() override;

    void run() override;
    float deltaTime() const override { return mDeltaTime; }
    int width() const override { return mWidth; }
    int height() const override { return mHeight; }
    void setTitle(const std::string& title) override;
    void close() override;

    GLFWwindow* glfwWindow() const { return mWindow; }

private:
    static void framebufferSizeCallback(GLFWwindow* win, int w, int h);
    static void keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods);
    static void mouseButtonCallback(GLFWwindow* win, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* win, double x, double y);
    static void scrollCallback(GLFWwindow* win, double xoffset, double yoffset);

    GLFWwindow* mWindow = nullptr;
    int mWidth, mHeight;
    float mLastTime = 0, mDeltaTime = 0;
};
