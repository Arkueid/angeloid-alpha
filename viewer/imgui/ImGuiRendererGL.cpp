#include "imgui/ImGuiRendererGL.h"

#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <glad/glad.h>

#include <imgui.h>
#include "backends/imgui_impl_opengl3.h"

bool ImGuiRendererGL::init(GLFWwindow*) {
    return ImGui_ImplOpenGL3_Init("#version 330 core");
}

void ImGuiRendererGL::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
}

void ImGuiRendererGL::render() {
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiRendererGL::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
}
