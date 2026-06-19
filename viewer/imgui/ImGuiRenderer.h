#pragma once

struct GLFWwindow;

class ImGuiRenderer {
public:
    virtual ~ImGuiRenderer() = default;

    virtual bool init(GLFWwindow* window) = 0;
    virtual void newFrame() = 0;
    virtual void render() = 0;
    virtual void shutdown() = 0;
};
