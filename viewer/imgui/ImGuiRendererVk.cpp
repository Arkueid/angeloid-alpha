#include "imgui/ImGuiRendererVk.h"
#include "framework/gpu/vulkan/VkDevice.h"

#include <imgui.h>
#include "backends/imgui_impl_vulkan.h"

ImGuiRendererVk::ImGuiRendererVk(Gpu::IGpuDevice* dev) : mDev(dev) {}

ImGuiRendererVk::~ImGuiRendererVk() {
    shutdown();
}

bool ImGuiRendererVk::init(GLFWwindow*) {
    auto* vkDev = static_cast<Gpu::VulkanDevice*>(mDev);

    mDynRender.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    mDynRender.colorAttachmentCount = 1;
    mDynRender.pColorAttachmentFormats = &mColorFormat;

    ImGui_ImplVulkan_InitInfo info{};
    info.ApiVersion = VK_API_VERSION_1_2;
    info.Instance = vkDev->vkInstance();
    info.PhysicalDevice = vkDev->physicalDevice();
    info.Device = vkDev->device();
    info.QueueFamily = vkDev->graphicsQueueFamily();
    info.Queue = vkDev->graphicsQueue();
    info.DescriptorPoolSize = 32;
    info.MinImageCount = 3;
    info.ImageCount = 3;
    info.UseDynamicRendering = true;
    info.PipelineInfoMain.PipelineRenderingCreateInfo = mDynRender;

    if (!ImGui_ImplVulkan_Init(&info))
        return false;

    mInitialized = true;
    return true;
}

void ImGuiRendererVk::newFrame() {
    ImGui_ImplVulkan_NewFrame();
}

void ImGuiRendererVk::render() {
    auto* vkDev = static_cast<Gpu::VulkanDevice*>(mDev);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkDev->currentCmd());
}

void ImGuiRendererVk::shutdown() {
    if (mInitialized) {
        ImGui_ImplVulkan_Shutdown();
        mInitialized = false;
    }
}
