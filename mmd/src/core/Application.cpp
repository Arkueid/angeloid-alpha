#include "core/Application.h"

#include <iostream>

Application::Application(int width, int height, const std::string& title)
    : mWidth(width), mHeight(height)
{
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    mWindow = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!mWindow) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(mWindow);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD (OpenGL loader)");
    }

    std::cout << "OpenGL " << GLVersion.major << "." << GLVersion.minor << "\n";
    std::cout << "Renderer: " << glGetString(GL_RENDERER) << "\n";

    glEnable(GL_DEPTH_TEST);

    glfwSetFramebufferSizeCallback(mWindow, framebufferSizeCallback);
    glfwSetKeyCallback(mWindow, keyCallback);
    glfwSetMouseButtonCallback(mWindow, mouseButtonCallback);
    glfwSetCursorPosCallback(mWindow, cursorPosCallback);
    glfwSetScrollCallback(mWindow, scrollCallback);

    glfwSetWindowUserPointer(mWindow, this);

    mLastTime = static_cast<float>(glfwGetTime());
}

Application::~Application()
{
    if (mWindow) {
        glfwDestroyWindow(mWindow);
    }
    glfwTerminate();
}

void Application::run()
{
    while (!glfwWindowShouldClose(mWindow)) {
        float now = static_cast<float>(glfwGetTime());
        mDeltaTime = now - mLastTime;
        mLastTime = now;

        if (onUpdate) {
            onUpdate(mDeltaTime);
        }

        if (onRender) {
            onRender();
        } else {
            glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        glfwSwapBuffers(mWindow);
        glfwPollEvents();
    }
}

void Application::setTitle(const std::string& title)
{
    glfwSetWindowTitle(mWindow, title.c_str());
}

// --- Static callbacks ---

void Application::framebufferSizeCallback(GLFWwindow* win, int w, int h)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(win));
    app->mWidth = w;
    app->mHeight = h;
    glViewport(0, 0, w, h);
    if (app->onResize) {
        app->onResize(w, h);
    }
}

void Application::keyCallback(GLFWwindow* win, int key, int scancode, int action, int mods)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(win));
    if (app->onKey) {
        app->onKey(key, scancode, action, mods);
    }
}

void Application::mouseButtonCallback(GLFWwindow* win, int button, int action, int mods)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(win));
    if (app->onMouseButton) {
        app->onMouseButton(button, action, mods);
    }
}

void Application::cursorPosCallback(GLFWwindow* win, double x, double y)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(win));
    if (app->onCursorPos) {
        app->onCursorPos(x, y);
    }
}

void Application::scrollCallback(GLFWwindow* win, double xoffset, double yoffset)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(win));
    if (app->onScroll) {
        app->onScroll(xoffset, yoffset);
    }
}
