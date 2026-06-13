#include "imgui/ImGuiManager.h"

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

bool ImGuiManager::init(GLFWwindow* window) {
    mWindow = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // no .ini file for now

    ImGui::StyleColorsDark();

    // Scale up from default ~13px to a comfortable size
    ImGui::GetStyle().ScaleAllSizes(2.0f);
    io.FontGlobalScale = 2.0f;

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
