#pragma once

#include <functional>
#include <string>

// Abstract window interface — decouples the application from GLFW/Qt/etc.
class IWindow {
   public:
    virtual ~IWindow() = default;

    virtual void run() = 0;
    virtual float deltaTime() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void setTitle(const std::string& title) = 0;
    virtual void close() = 0;

    std::function<void(float)> onUpdate;
    std::function<void()> onRender;
    std::function<void(int, int)> onResize;
    std::function<void(int, int, int, int)> onKey;
    std::function<void(int, int, int)> onMouseButton;
    std::function<void(double, double)> onCursorPos;
    std::function<void(double, double)> onScroll;
};
