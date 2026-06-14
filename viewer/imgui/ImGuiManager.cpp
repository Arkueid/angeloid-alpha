#include "imgui/ImGuiManager.h"

#include <filesystem>

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

static ImGuiManager* sInstance = nullptr;

ImGuiManager::~ImGuiManager() {
    shutdown();
}

bool ImGuiManager::init(GLFWwindow* window, const char* cjkFontPath) {
    mWindow = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();

    // CJK font: covers Latin, Japanese (Hiragana/Katakana), Chinese ideographs
    static const ImWchar cjkRanges[] = {
        0x0020, 0x00FF,  // Basic Latin + Latin Supplement
        0x2000, 0x206F,  // General Punctuation
        0x3000, 0x303F,  // CJK Symbols and Punctuation
        0x3040, 0x309F,  // Hiragana
        0x30A0, 0x30FF,  // Katakana
        0x4E00, 0x9FFF,  // CJK Unified Ideographs
        0xFF00, 0xFFEF,  // Halfwidth and Fullwidth Forms
        0,
    };
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    float fontSize = 13.0f * yscale;
    if (cjkFontPath && std::filesystem::exists(cjkFontPath))
        io.Fonts->AddFontFromFileTTF(cjkFontPath, fontSize, nullptr, cjkRanges);
    ImGui::GetStyle().ScaleAllSizes(yscale);

    // false = don't install GLFW callbacks; we forward manually
    if (!ImGui_ImplGlfw_InitForOpenGL(window, false))
        return false;
    if (!ImGui_ImplOpenGL3_Init("#version 330 core"))
        return false;

    // Install char callback for text input (GlfwWindow doesn't forward it)
    sInstance = this;
    glfwSetCharCallback(window, charCallback);

    mInitialized = true;
    return true;
}

void ImGuiManager::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiManager::shutdown() {
    if (!mInitialized)
        return;

    if (mWindow) {
        glfwSetCharCallback(mWindow, nullptr);
        mWindow = nullptr;
    }

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

// static
void ImGuiManager::charCallback(GLFWwindow* /*window*/, unsigned int codepoint) {
    if (sInstance)
        sInstance->onChar(codepoint);
}
