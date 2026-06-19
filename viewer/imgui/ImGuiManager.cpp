#include "imgui/ImGuiManager.h"
#include "imgui/ImGuiRenderer.h"

#include <filesystem>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include "backends/imgui_impl_glfw.h"

static ImGuiManager* sInstance = nullptr;

ImGuiManager::~ImGuiManager() {
    shutdown();
}

bool ImGuiManager::init(GLFWwindow* window, std::unique_ptr<ImGuiRenderer> renderer,
                        bool useVulkan, const char* cjkFontPath) {
    mWindow = window;

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
    bool glfwOk = useVulkan
        ? ImGui_ImplGlfw_InitForVulkan(window, false)
        : ImGui_ImplGlfw_InitForOpenGL(window, false);
    if (!glfwOk) return false;

    if (!renderer->init(window)) return false;

    mRenderer = std::move(renderer);
    sInstance = this;
    glfwSetCharCallback(window, charCallback);

    mInitialized = true;
    return true;
}

void ImGuiManager::beginFrame() {
    mRenderer->newFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::endFrame() {
    ImGui::Render();
    mRenderer->render();
}

void ImGuiManager::shutdown() {
    if (!mInitialized) return;

    if (mWindow) {
        glfwSetCharCallback(mWindow, nullptr);
        mWindow = nullptr;
    }

    if (mRenderer) {
        mRenderer->shutdown();
        mRenderer = nullptr;
    }

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
