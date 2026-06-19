#pragma once

struct GLFWwindow;

namespace Gpu { class IGpuDevice; }

class ImGuiManager {
public:
    ImGuiManager() = default;
    ~ImGuiManager();

    bool init(GLFWwindow* window, Gpu::IGpuDevice* dev,
              const char* cjkFontPath = nullptr);

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
    bool mVulkan = false;
    GLFWwindow* mWindow = nullptr;
};
