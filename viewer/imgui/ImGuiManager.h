#pragma once

struct GLFWwindow;

class ImGuiManager {
public:
    ImGuiManager() = default;
    ~ImGuiManager();

    bool init(GLFWwindow* window);
    void beginFrame();
    void endFrame();
    void shutdown();

    bool initialized() const { return mInitialized; }

    // Manual event forwarders (install_callers=false)
    void onKey(int key, int scancode, int action, int mods);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);
    void onChar(unsigned int codepoint);

private:
    static void charCallback(GLFWwindow* window, unsigned int codepoint);

    bool mInitialized = false;
    GLFWwindow* mWindow = nullptr;
};
