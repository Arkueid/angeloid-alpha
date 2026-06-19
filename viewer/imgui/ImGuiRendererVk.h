#pragma once

#include "imgui/ImGuiRenderer.h"

#include <vulkan/vulkan.h>

namespace Gpu { class IGpuDevice; }

class ImGuiRendererVk : public ImGuiRenderer {
public:
    explicit ImGuiRendererVk(Gpu::IGpuDevice* dev);
    ~ImGuiRendererVk() override;

    bool init(GLFWwindow* window) override;
    void newFrame() override;
    void render() override;
    void shutdown() override;

private:
    Gpu::IGpuDevice* mDev;
    bool mInitialized = false;
    VkFormat mColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkPipelineRenderingCreateInfoKHR mDynRender{};
};
