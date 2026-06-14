#include "framework/MMD.h"
#include "framework/Camera.h"
#include "framework/Model.h"
#include "framework/opengl/Pipeline.h"
#include "framework/opengl/scene/GroundPlane.h"
#include "framework/opengl/scene/WorldAxis.h"
#include "framework/util/CfgParser.h"
#include "window/GlfwWindow.h"
#include "imgui/ImGuiManager.h"
#include <imgui.h>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

static void printHelp() {
    std::cout << "\nCamera Controls:\n"
                 "  M key: Toggle between FPS and Orbit camera mode\n"
                 "\nFPS Camera Controls:\n"
                 "  Left mouse drag: Rotate camera view\n"
                 "  W/A/S/D: Move forward/left/backward/right\n"
                 "  E/Q: Move up/down\n"
                 "  Mouse scroll: Adjust movement speed\n"
                 "  X key: Toggle world axis display\n"
                 "  G key: Toggle ground grid display\n"
                 "  B key: Toggle rigidbody & joint display\n"
                 "  H key: Toggle model mesh display\n"
                 "  O key: Toggle outline display\n"
                 "  T key: Toggle toon shading\n"
                 "  P key: Toggle VPD pose\n"
                 "  R key: Reset camera to default position\n"
                 "Orbit Camera Controls:\n"
                 "  Left mouse drag: Orbit around target\n"
                 "  Scroll: Dolly (zoom in/out)\n"
                 "  Middle mouse drag / Shift+Left: Pan (move target)\n"
                 "  R key: Reset camera\n"
                 "  I key: Toggle idle animation\n"
                 "  < / > keys: Switch between morphs\n"
                 "  Up/Down keys: Adjust morph weight\n"
                 "VMD Animation Controls:\n"
                 "  Space: Play/Pause VMD animation\n"
                 "  L key: Toggle VMD loop\n"
                 "  [ / ] keys: Step backward/forward 30 frames\n"
              << std::endl;
}

static std::unordered_map<std::string, std::string> loadModelRegistry(const std::filesystem::path& cfgPath) {
    auto map = parseCfgFile(cfgPath);
    if (map.empty()) {
        std::cerr << "WARNING: No models found in " << cfgPath.string() << std::endl;
    }
    return map;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> u8args(argc);
    for (int i = 0; i < argc; ++i) {
        int wlen = MultiByteToWideChar(CP_ACP, 0, argv[i], -1, nullptr, 0);
        std::vector<wchar_t> wide(wlen);
        MultiByteToWideChar(CP_ACP, 0, argv[i], -1, wide.data(), wlen);
        int ulen = WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, nullptr, 0, nullptr, nullptr);
        u8args[i].resize(ulen - 1);
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), -1, &u8args[i][0], ulen, nullptr, nullptr);
    }
    std::vector<char*> u8argv(argc);
    for (int i = 0; i < argc; ++i)
        u8argv[i] = u8args[i].data();
    argv = u8argv.data();
#endif
    std::cout << "MMD PMX Viewer (C++)" << std::endl;

    std::string modelName = "ikaros-uniform";
    std::vector<fs::path> vmdPaths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--model" || arg == "-m") && i + 1 < argc)
            modelName = argv[++i];
        else if (arg == "--vmd" || arg == "-v")
            while (i + 1 < argc && argv[i + 1][0] != '-')
                vmdPaths.push_back(fs::u8path(argv[++i]));
        else if (arg[0] != '-')
            modelName = arg;
    }

    fs::path projRoot = fs::weakly_canonical(fs::path(MMD_PROJECT_ROOT));
    auto modelRegistry = loadModelRegistry(projRoot / "resources/models.cfg");

    fs::path pmxPath;
    auto it = modelRegistry.find(modelName);
    if (it != modelRegistry.end())
        pmxPath = projRoot / fs::u8path(it->second);
    else
        pmxPath = fs::u8path(modelName);
    fs::path vpdPath = projRoot / fs::u8path("resources/vpd/自然站姿.vpd");
    // --- Window ---
    GlfwWindow app(1280, 720, "MMD PMX Viewer");

    ImGuiManager imgui;
    imgui.init(app.glfwWindow());

    // --- Init mmd module ---
    mmd::InitArgs args;
    args.shaderDir = projRoot / "resources/shaders";
    args.toonDir = projRoot / "resources/toon";
    args.effectsCfg = projRoot / "resources/effects.cfg";
    args.blinkMorphs = {"blink", "blink_l", "blink_r",
                        "まばたき", "まぶたき", "ウィンク", "ｳｨﾝｸ"};
    mmd::init(std::move(args));

    // --- Load model ---
    mmd::Model model;
    model.load(pmxPath);
    app.setTitle("MMD PMX Viewer - " + model.modelName());
    int activeVpdId = -1;
    if (!vpdPath.empty()) {
        activeVpdId = model.loadVpd(vpdPath);
        if (activeVpdId >= 0)
            model.applyVpd(activeVpdId);
    }

    // --- VMD ---
    std::vector<int> vmdTrackIds;
    for (auto& vp : vmdPaths) {
        if (vp.is_relative())
            vp = projRoot / vp;
        int id = model.loadVmd(vp);
        vmdTrackIds.push_back(id);
        if (id >= 0) {
            model.playVmd(id, [&](int id) {
                std::cout << "VMD track " << id << " finished playing." << std::endl;
                model.syncVpdPose();
            });
        }
    }

    WorldAxis worldAxis;
    GroundPlane groundPlane;

    auto& pipe = Pipeline::instance();
    pipe.addRenderable(&groundPlane);
    pipe.addRenderable(&worldAxis);
    pipe.addRenderable(&model);

    printHelp();

    // Morph state
    int morphIndex = -1;
    float morphWeight = 0.0f;
    auto morphList = model.interactableMorphs();

    // Input
    auto& cam = Camera::instance();
    app.onMouseButton = [&](int b, int a, int m) {
        imgui.onMouseButton(b, a, m);
        if (!ImGui::GetIO().WantCaptureMouse)
            cam.onMouseButton(b, a, m);
    };
    app.onCursorPos = [&](double x, double y) {
        if (!ImGui::GetIO().WantCaptureMouse)
            cam.onCursorPos(x, y);
    };
    app.onScroll = [&](double xo, double yo) {
        imgui.onScroll(xo, yo);
        if (!ImGui::GetIO().WantCaptureMouse)
            cam.onScroll(yo);
    };
    app.onKey = [&](int key, int sc, int act, int mods) {
        imgui.onKey(key, sc, act, mods);
        if (ImGui::GetIO().WantCaptureKeyboard)
            return;
        if (key == GLFW_KEY_ESCAPE && act == GLFW_PRESS)
            app.close();
        if (key == GLFW_KEY_X && act == GLFW_PRESS)
            worldAxis.showAxis = !worldAxis.showAxis;
        if (key == GLFW_KEY_G && act == GLFW_PRESS)
            worldAxis.showGrid = !worldAxis.showGrid;
        if (key == GLFW_KEY_B && act == GLFW_PRESS) {
            model.showRigidBodies(!model.showRigidBodies());
        }
        if (key == GLFW_KEY_H && act == GLFW_PRESS)
            model.showModel(!model.showModel());
        if (key == GLFW_KEY_O && act == GLFW_PRESS)
            model.showOutline(!model.showOutline());
        if (key == GLFW_KEY_T && act == GLFW_PRESS)
            model.showToon(!model.showToon());
        if (key == GLFW_KEY_P && act == GLFW_PRESS) {
            if (activeVpdId >= 0 && model.vpdApplied()) {
                model.resetPose();
                std::cout << "VPD pose: OFF" << std::endl;
            } else if (activeVpdId >= 0) {
                model.applyVpd(activeVpdId);
                std::cout << "VPD pose: ON" << std::endl;
            }
        }
        if (key == GLFW_KEY_Y && act == GLFW_PRESS) {
            model.enablePhysics(!model.physicsEnabled());
            std::cout << "Physics: " << (model.physicsEnabled() ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_M && act == GLFW_PRESS) {
            cam.toggleMode();
            std::cout << "Camera mode: " << (cam.mode() == CameraMode::FPS ? "FPS" : "Orbit") << std::endl;
        }
        if (key == GLFW_KEY_R && act == GLFW_PRESS)
            cam.reset();
        if (key == GLFW_KEY_I && act == GLFW_PRESS) {
            static bool idle = true;
            idle = !idle;
            model.setIdleBlink(idle);
            if (!idle)
                model.clearMorphs();
        }
        if (key == GLFW_KEY_COMMA && act == GLFW_PRESS) {
            if (!morphList.empty()) {
                auto it = std::find(morphList.begin(), morphList.end(), morphIndex);
                int idx = it != morphList.end() ? (int)(it - morphList.begin()) : 0;
                idx = (idx - 1 + (int)morphList.size()) % (int)morphList.size();
                morphIndex = morphList[idx];
                std::string name = model.morphName(morphIndex);
                MMD_INFO("MORPH", "select [%d/%d] %s", idx, (int)morphList.size(), name.c_str());
                morphWeight = model.savedMorphWeight(name);
                model.setMorphWeight(name, morphWeight);
            }
        }
        if (key == GLFW_KEY_PERIOD && act == GLFW_PRESS) {
            if (!morphList.empty()) {
                auto it = std::find(morphList.begin(), morphList.end(), morphIndex);
                int idx = it != morphList.end() ? (int)(it - morphList.begin()) : 0;
                idx = (idx + 1) % (int)morphList.size();
                morphIndex = morphList[idx];
                std::string name = model.morphName(morphIndex);
                MMD_INFO("MORPH", "select [%d/%d] %s", idx, (int)morphList.size(), name.c_str());
                morphWeight = model.savedMorphWeight(name);
                model.setMorphWeight(name, morphWeight);
            }
        }
        if (key == GLFW_KEY_UP && act != GLFW_RELEASE) {
            if (morphIndex >= 0 && morphIndex < model.morphCount()) {
                morphWeight = std::min(1.0f, morphWeight + 0.1f);
                auto name = model.morphName(morphIndex);
                model.setMorphWeight(name, morphWeight);
            }
        }
        if (key == GLFW_KEY_DOWN && act != GLFW_RELEASE) {
            if (morphIndex >= 0 && morphIndex < model.morphCount()) {
                morphWeight = std::max(0.0f, morphWeight - 0.1f);
                auto name = model.morphName(morphIndex);
                model.setMorphWeight(name, morphWeight);
            }
        }
        if (key == GLFW_KEY_SPACE && act == GLFW_PRESS) {
            if (!vmdTrackIds.empty()) {
                if (model.isVmdPlaying())
                    model.pauseAllVmd();
                else
                    model.playAllVmd();
            }
        }
        if (key == GLFW_KEY_L && act == GLFW_PRESS) {
            // Re-play (stop + replay first track)
            if (!vmdTrackIds.empty()) {
                model.stopAllVmd();
                model.playAllVmd();
            }
        }
        if (key == GLFW_KEY_LEFT_BRACKET && act != GLFW_RELEASE) {
            if (!vmdTrackIds.empty())
                for (int id : vmdTrackIds)
                    model.setVmdFrame(id, model.vmdCurrentFrame(id) - 30);
        }
        if (key == GLFW_KEY_RIGHT_BRACKET && act != GLFW_RELEASE) {
            if (!vmdTrackIds.empty())
                for (int id : vmdTrackIds)
                    model.setVmdFrame(id, model.vmdCurrentFrame(id) + 30);
        }
    };

    // Update
    app.onUpdate = [&](float dt) {
        imgui.beginFrame();

        // ---- Debug Panel ----
        static float smoothFps = 0.0f;
        smoothFps = smoothFps * 0.95f + (1.0f / dt) * 0.05f;
        if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("FPS: %.0f (%.2f ms)", smoothFps, dt * 1000.0);
            ImGui::SeparatorText("Model");
            ImGui::Text("Name: %s", model.modelName().c_str());
            ImGui::Text("Vertices: %d", model.data().vertexCount());
            ImGui::Text("Faces: %d", model.data().faceCount());
            ImGui::Text("Bones: %d", model.data().boneCount());
            ImGui::Text("Morphs: %d", model.morphCount());
            ImGui::SeparatorText("Render");
            bool v = model.showModel();
            if (ImGui::Checkbox("Model", &v)) model.showModel(v);
            v = model.showOutline();
            if (ImGui::Checkbox("Outline", &v)) model.showOutline(v);
            v = model.showToon();
            if (ImGui::Checkbox("Toon", &v)) model.showToon(v);
            v = model.physicsEnabled();
            if (ImGui::Checkbox("Physics", &v)) model.enablePhysics(v);
            v = groundPlane.visible;
            if (ImGui::Checkbox("Ground", &v)) groundPlane.visible = v;
        }
        ImGui::End();

        auto* win = app.glfwWindow();
        if (cam.mode() == CameraMode::FPS) {
            cam.update(
                dt, glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS,
                glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS, glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS,
                glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS, glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS,
                glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS);
        }
        model.update(dt);
    };

    // Render
    app.onRender = [&]() {
        pipe.resizeViewport(app.width(), app.height());

        auto proj = Camera::projectionMatrix(app.width(), app.height());
        auto view = Camera::instance().viewMatrix();

        float lightDir[3] = {0.3f, 0.8f, 0.5f};
        Vec3 sceneMin, sceneMax;
        model.worldAABB(sceneMin, sceneMax);
        pipe.computeLightMatrix(lightDir, &sceneMin.x, &sceneMax.x);
        pipe.execute(proj, view);

        imgui.endFrame();
    };

    app.run();
    glFinish();
    mmd::dispose();
    return 0;
}
