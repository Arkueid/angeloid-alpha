#include "window/GlfwWindow.h"

#include <iostream>
#include <stdexcept>

GlfwWindow::GlfwWindow(int width, int height, const std::string& title)
    : mWidth(width), mHeight(height) {
    if (!glfwInit())
        throw std::runtime_error("Failed to initialize GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_SAMPLES, 4);

    mWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!mWindow) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(mWindow);

    if (!gladLoadGL()) {
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD (OpenGL loader)");
    }

    std::cout << "OpenGL " << GLVersion.major << "." << GLVersion.minor << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    glfwSetFramebufferSizeCallback(mWindow, framebufferSizeCallback);
    glfwSetKeyCallback(mWindow, keyCallback);
    glfwSetMouseButtonCallback(mWindow, mouseButtonCallback);
    glfwSetCursorPosCallback(mWindow, cursorPosCallback);
    glfwSetScrollCallback(mWindow, scrollCallback);

    glfwSetWindowUserPointer(mWindow, this);

    mLastTime = static_cast<float>(glfwGetTime());
}

GlfwWindow::~GlfwWindow() {
    if (mWindow)
        glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void GlfwWindow::run() {
    while (!glfwWindowShouldClose(mWindow)) {
        float now = static_cast<float>(glfwGetTime());
        mDeltaTime = now - mLastTime;
        mLastTime = now;

        if (onUpdate)
            onUpdate(mDeltaTime);

        if (onRender) {
            onRender();
        }
        else {
            glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        glfwSwapBuffers(mWindow);
        glfwPollEvents();
    }
}

void GlfwWindow::setTitle(const std::string& title) {
    glfwSetWindowTitle(mWindow, title.c_str());
}

void GlfwWindow::close() {
    glfwSetWindowShouldClose(mWindow, GLFW_TRUE);
}

void GlfwWindow::framebufferSizeCallback(GLFWwindow* win, int w, int h) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    self->mWidth = w;
    self->mHeight = h;
    glViewport(0, 0, w, h);
    if (self->onResize)
        self->onResize(w, h);
}

void GlfwWindow::keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (self->onKey)
        self->onKey(key, scancode, action, mods);
}

void GlfwWindow::mouseButtonCallback(GLFWwindow* win, int button, int action, int mods) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (self->onMouseButton)
        self->onMouseButton(button, action, mods);
}

void GlfwWindow::cursorPosCallback(GLFWwindow* win, double x, double y) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (self->onCursorPos)
        self->onCursorPos(x, y);
}

void GlfwWindow::scrollCallback(GLFWwindow* win, double xoffset, double yoffset) {
    auto* self = static_cast<GlfwWindow*>(glfwGetWindowUserPointer(win));
    if (self->onScroll)
        self->onScroll(xoffset, yoffset);
}
