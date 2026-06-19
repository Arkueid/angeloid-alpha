#include "imgui/ImGuiManager.h"

#include <filesystem>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "framework/gpu/vulkan/VkDevice.h"

static ImGuiManager* sInstance = nullptr;

ImGuiManager::~ImGuiManager() {
    shutdown();
}

bool ImGuiManager::init(GLFWwindow* window, Gpu::IGpuDevice* dev,
                        const char* cjkFontPath) {
    mWindow = window;
    mVulkan = (dev->asVulkan() != nullptr);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    static const ImWchar cjkRanges[] = {
        0x0020, 0x00FF,  0x2000, 0x206F,  0x3000, 0x303F,
        0x3040, 0x309F,  0x30A0, 0x30FF,  0x4E00, 0x9FFF,
        0xFF00, 0xFFEF,  0,
    };
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    float fontSize = 13.0f * yscale;
    if (cjkFontPath && std::filesystem::exists(cjkFontPath))
        io.Fonts->AddFontFromFileTTF(cjkFontPath, fontSize, nullptr, cjkRanges);
    ImGui::GetStyle().ScaleAllSizes(yscale);

    // GLFW platform backend
    if (mVulkan)
        ImGui_ImplGlfw_InitForVulkan(window, false);
    else
        ImGui_ImplGlfw_InitForOpenGL(window, false);

    // Renderer backend
    if (mVulkan) {
        auto* vkDev = dev->asVulkan();
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
        info.PipelineInfoMain.MSAASamples = vkDev->msaaSamples();

        VkPipelineRenderingCreateInfoKHR dynRender{};
        dynRender.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        dynRender.colorAttachmentCount = 1;
        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        dynRender.pColorAttachmentFormats = &colorFormat;
        info.PipelineInfoMain.PipelineRenderingCreateInfo = dynRender;

        if (!ImGui_ImplVulkan_Init(&info))
            return false;
    } else {
        if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
            return false;
    }

    sInstance = this;
    glfwSetCharCallback(window, charCallback);

    mInitialized = true;
    return true;
}

void ImGuiManager::beginFrame() {
    if (mVulkan)
        ImGui_ImplVulkan_NewFrame();
    else
        ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::endFrame() {
    ImGui::Render();
    if (mVulkan)
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
            Gpu::device()->asVulkan()->currentCmd());
    else
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiManager::shutdown() {
    if (!mInitialized) return;

    if (mWindow) {
        glfwSetCharCallback(mWindow, nullptr);
        mWindow = nullptr;
    }

    if (mVulkan)
        ImGui_ImplVulkan_Shutdown();
    else
        ImGui_ImplOpenGL3_Shutdown();

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    sInstance = nullptr;
    mInitialized = false;
}

void ImGuiManager::onKey(int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(mWindow, key, scancode, action, mods);
}

void ImGuiManager::onMouseButton(int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(mWindow, button, action, mods);
}

void ImGuiManager::onScroll(double xoffset, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(mWindow, xoffset, yoffset);
}

void ImGuiManager::onChar(unsigned int codepoint) {
    ImGui_ImplGlfw_CharCallback(mWindow, codepoint);
}

void ImGuiManager::charCallback(GLFWwindow*, unsigned int codepoint) {
    if (sInstance) sInstance->onChar(codepoint);
}
