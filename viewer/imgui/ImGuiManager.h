#pragma once

#include <memory>

struct GLFWwindow;

class ImGuiRenderer;

class ImGuiManager {
public:
    ImGuiManager() = default;
    ~ImGuiManager();

    // Takes ownership of renderer. useVulkan = use ImGui_ImplGlfw_InitForVulkan.
    bool init(GLFWwindow* window, std::unique_ptr<ImGuiRenderer> renderer,
              bool useVulkan, const char* cjkFontPath = nullptr);

    void beginFrame();
    void endFrame();
    void shutdown();

    bool initialized() const { return mInitialized; }

    void onKey(int key, int scancode, int action, int mods);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);
    void onChar(unsigned int codepoint);

private:
    static void charCallback(GLFWwindow* window, unsigned int codepoint);

    bool mInitialized = false;
    GLFWwindow* mWindow = nullptr;
    std::unique_ptr<ImGuiRenderer> mRenderer;
};
