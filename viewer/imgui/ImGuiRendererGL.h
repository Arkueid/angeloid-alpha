#pragma once

#include "imgui/ImGuiRenderer.h"

class ImGuiRendererGL : public ImGuiRenderer {
public:
    bool init(GLFWwindow* window) override;
    void newFrame() override;
    void render() override;
    void shutdown() override;
};
