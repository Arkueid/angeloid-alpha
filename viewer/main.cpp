#include "framework/Camera.h"
#include "framework/MMD.h"
#include "framework/Model.h"
#include "framework/gpu/IGpuDevice.h"
#include "framework/Pipeline.h"
#include "framework/scene/GroundPlane.h"
#include "framework/scene/WorldAxis.h"
#include "framework/util/CfgParser.h"
#include "imgui/ImGuiManager.h"
#include "window/GlfwWindow.h"

#include <imgui.h>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
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

static std::unordered_map<std::string, std::string> loadModelRegistry(
    const std::filesystem::path& cfgPath) {
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
    mmd::GpuBackend backend = mmd::GpuBackend::Vulkan;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--model" || arg == "-m") && i + 1 < argc)
            modelName = argv[++i];
        else if ((arg == "--opengl" || arg == "--gl"))
            backend = mmd::GpuBackend::OpenGL;
        else if ((arg == "--vulkan" || arg == "--vk"))
            backend = mmd::GpuBackend::Vulkan;
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
    GlfwWindow app(1280, 720, "MMD PMX Viewer", backend);

    // --- Init mmd module ---
    mmd::InitArgs args;
    if (backend == mmd::GpuBackend::Vulkan) {
        args.shaderDir = projRoot / "resources/shaders/vulkan";
        args.effectsCfg = projRoot / "resources/effects.cfg";  // same cfg, same shader names
    } else {
        args.shaderDir = projRoot / "resources/shaders/opengl";
        args.effectsCfg = projRoot / "resources/effects.cfg";
    }
    args.toonDir = projRoot / "resources/toon";
    args.blinkMorphs = {"blink", "blink_l", "blink_r", "まばたき", "まぶたき", "ウィンク", "ｳｨﾝｸ"};
    args.backend = backend;
    args.window = app.glfwWindow();
    mmd::init(std::move(args));

    // --- ImGui (must follow mmd::init, needs GPU device alive) ---
    ImGuiManager imgui;
    imgui.init(app.glfwWindow(), mmd::gpuDevice(),
               (projRoot / "resources/fonts/cjk.ttf").string().c_str());

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

    // Shared UI state
    int morphIndex = -1;
    float morphWeight = 0.0f;
    auto morphList = model.interactableMorphs();
    bool idleBlink = true;

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
            }
            else if (activeVpdId >= 0) {
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
            std::cout << "Camera mode: " << (cam.mode() == CameraMode::FPS ? "FPS" : "Orbit")
                      << std::endl;
        }
        if (key == GLFW_KEY_R && act == GLFW_PRESS)
            cam.reset();
        if (key == GLFW_KEY_I && act == GLFW_PRESS) {
            idleBlink = !idleBlink;
            model.setIdleBlink(idleBlink);
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

        // ---- Control Panel ----
        static float smoothFps = 0.0f;
        smoothFps = smoothFps * 0.95f + (1.0f / dt) * 0.05f;
        ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Model: %s", model.modelName().c_str());
        ImGui::Text("GPU: %s", mmd::gpuDevice()->asVulkan() ? "Vulkan" : "OpenGL");
        ImGui::Text("FPS: %.0f (%.2f ms)", smoothFps, dt * 1000.0);

        // ── Model Inspector ──
        if (ImGui::CollapsingHeader("Model Inspector")) {
            char buf[64];
            auto& pmx = model.data();

            snprintf(buf, sizeof(buf), "Details: %s", model.modelName().c_str());
            if (ImGui::TreeNode(buf)) {
                if (ImGui::BeginChild("##details", ImVec2(0, 160), true)) {
                    ImGui::Text("Verts: %d", pmx.vertexCount());
                    ImGui::Text("Faces: %d", pmx.faceCount());
                    ImGui::Text("Bones: %d", pmx.boneCount());
                    ImGui::Text("Morphs: %d", pmx.morphCount());
                    ImGui::Text("Materials: %zu", pmx.materials.size());
                    ImGui::Text("Textures: %zu", pmx.textures.size());
                    ImGui::Text("RigidBodies: %zu", pmx.rigidbodies.size());
                    ImGui::Text("Joints: %zu", pmx.joints.size());
                }
                ImGui::EndChild();
                ImGui::TreePop();
            }

            snprintf(buf, sizeof(buf), "Bones (%d)", pmx.boneCount());
            if (ImGui::TreeNode(buf)) {
                int shown = 0;
                if (ImGui::BeginChild("##bnscroll", ImVec2(0, 160), true)) {
                    for (int i = 0; i < pmx.boneCount(); ++i) {
                        auto& b = pmx.bones[i];
                        if (++shown > 500) {
                            ImGui::TextDisabled("...");
                            break;
                        }
                        ImGui::Text("%4d: %-24s parent=%d flags=0x%04X", i, b.name.c_str(),
                                    b.parent_index, b.flag);
                    }
                }
                ImGui::EndChild();
                ImGui::TreePop();
            }

            snprintf(buf, sizeof(buf), "RigidBodies (%zu)", pmx.rigidbodies.size());
            if (ImGui::TreeNode(buf)) {
                if (ImGui::BeginChild("##rbscrl", ImVec2(0, 160), true)) {
                    for (size_t i = 0; i < pmx.rigidbodies.size(); ++i) {
                        auto& rb = pmx.rigidbodies[i];
                        const char* s = rb.shape_type == 0   ? "Sphere"
                                        : rb.shape_type == 1 ? "Box"
                                                             : "Capsule";
                        ImGui::Text("%3zu: %-24s %-7s bone=%d group=%d", i, rb.name.c_str(), s,
                                    rb.bone_index, rb.collision_group);
                    }
                }
                ImGui::EndChild();
                ImGui::TreePop();
            }

            snprintf(buf, sizeof(buf), "Joints (%zu)", pmx.joints.size());
            if (ImGui::TreeNode(buf)) {
                if (ImGui::BeginChild("##jtscrl", ImVec2(0, 160), true)) {
                    for (size_t i = 0; i < pmx.joints.size(); ++i) {
                        auto& jt = pmx.joints[i];
                        ImGui::Text("%3zu: %-24s type=%d  %d<->%d", i, jt.name.c_str(),
                                    jt.joint_type, jt.rigidbody_index_a, jt.rigidbody_index_b);
                    }
                }
                ImGui::EndChild();
                ImGui::TreePop();
            }

            snprintf(buf, sizeof(buf), "Morphs (%d)", pmx.morphCount());
            if (ImGui::TreeNode(buf)) {
                static char morphFilter[64] = "";
                ImGui::InputText("Filter", morphFilter, sizeof(morphFilter));
                int mshown = 0;
                if (ImGui::BeginChild("##mpscroll", ImVec2(0, 160), true)) {
                    for (int i = 0; i < pmx.morphCount(); ++i) {
                        auto& m = pmx.morphs[i];
                        if (morphFilter[0] && !strstr(m.name.c_str(), morphFilter) &&
                            !strstr(m.english_name.c_str(), morphFilter))
                            continue;
                        if (++mshown > 500) {
                            ImGui::TextDisabled("...");
                            break;
                        }
                        const char* t = m.morph_type == MORPH_TYPE_GROUP      ? "Group"
                                        : m.morph_type == MORPH_TYPE_VERTEX   ? "Vertex"
                                        : m.morph_type == MORPH_TYPE_BONE     ? "Bone"
                                        : m.morph_type == MORPH_TYPE_UV       ? "UV"
                                        : m.morph_type == MORPH_TYPE_MATERIAL ? "Material"
                                                                              : "?";
                        ImGui::Text("%4d: %-24s %s", i, m.name.c_str(), t);
                    }
                }
                ImGui::EndChild();
                ImGui::TreePop();
            }

            snprintf(buf, sizeof(buf), "Textures (%d)", (int)pmx.textures.size());
            if (ImGui::TreeNode(buf)) {
                if (ImGui::BeginChild("##texscrl", ImVec2(0, 160), true)) {
                    for (int i = 0; i < (int)pmx.textures.size(); ++i)
                        ImGui::Text("%3d: %s", i, pmx.textures[i].c_str());
                }
                ImGui::EndChild();
                ImGui::TreePop();
            }
        }

        // ── Display ──
        if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool v;
            v = model.showModel();
            if (ImGui::Checkbox("Model (H)", &v))
                model.showModel(v);
            v = model.showOutline();
            if (ImGui::Checkbox("Outline (O)", &v))
                model.showOutline(v);
            v = model.showToon();
            if (ImGui::Checkbox("Toon (T)", &v))
                model.showToon(v);
            v = model.physicsEnabled();
            if (ImGui::Checkbox("Physics (Y)", &v))
                model.enablePhysics(v);
            v = model.showRigidBodies();
            if (ImGui::Checkbox("RigidBody (B)", &v))
                model.showRigidBodies(v);
            v = pipe.showSelfShadow;
            if (ImGui::Checkbox("Self Shadow", &v))
                pipe.showSelfShadow = v;
            v = pipe.showGroundShadow;
            if (ImGui::Checkbox("Ground Shadow", &v))
                pipe.showGroundShadow = v;
            v = groundPlane.visible;
            if (ImGui::Checkbox("Ground", &v))
                groundPlane.visible = v;
            v = worldAxis.showAxis;
            if (ImGui::Checkbox("Axis (X)", &v))
                worldAxis.showAxis = v;
            v = worldAxis.showGrid;
            if (ImGui::Checkbox("Grid (G)", &v))
                worldAxis.showGrid = v;
        }

        // ── VMD Animation ──
        if (ImGui::CollapsingHeader("VMD Animation")) {
            int nTracks = model.vmdTrackCount();
            if (nTracks == 0) {
                ImGui::TextDisabled("No VMD loaded");
            }
            else {
                bool playing = model.isVmdPlaying();
                if (ImGui::Button(playing ? "Pause (Space)" : "Play (Space)")) {
                    if (playing)
                        model.pauseAllVmd();
                    else
                        model.playAllVmd();
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop"))
                    model.stopAllVmd();

                ImGui::SameLine();
                if (ImGui::Button("[ << ]")) {
                    for (int id : vmdTrackIds)
                        model.setVmdFrame(id, model.vmdCurrentFrame(id) - 30);
                }
                ImGui::SameLine();
                if (ImGui::Button("[ >> ]")) {
                    for (int id : vmdTrackIds)
                        model.setVmdFrame(id, model.vmdCurrentFrame(id) + 30);
                }

                int shown = 0;
                for (int id : vmdTrackIds) {
                    if (++shown > 4)
                        break;
                    char label[32];
                    snprintf(label, sizeof(label), "Track %d", id);
                    float cur = model.vmdCurrentFrame(id);
                    float maxF = model.vmdMaxFrame(id);
                    if (maxF > 0) {
                        float f = cur;
                        ImGui::SliderFloat(label, &f, 0.0f, maxF, "%.0f");
                        if (f != cur)
                            model.setVmdFrame(id, f);
                    }
                    else {
                        ImGui::Text("%s: %.0f", label, cur);
                    }
                }
                if ((int)vmdTrackIds.size() > 4)
                    ImGui::TextDisabled("... %d more tracks", (int)vmdTrackIds.size() - 4);
            }

            // VPD
            bool vpdOn = model.vpdApplied();
            if (ImGui::Button(vpdOn ? "VPD Pose: OFF (P)" : "VPD Pose: ON (P)")) {
                if (activeVpdId >= 0 && model.vpdApplied())
                    model.resetPose();
                else if (activeVpdId >= 0)
                    model.applyVpd(activeVpdId);
            }
        }

        // ── Morphs ──
        if (ImGui::CollapsingHeader("Morphs")) {
            if (ImGui::Checkbox("Idle Blink (I)", &idleBlink))
                model.setIdleBlink(idleBlink);
            ImGui::SameLine();
            if (ImGui::Button("Reset All"))
                model.clearMorphs();

            if (morphList.empty()) {
                ImGui::TextDisabled("No interactable morphs");
            }
            else {
                static std::vector<std::string> morphNames;
                static std::vector<const char*> morphCStrs;
                if (morphNames.size() != morphList.size()) {
                    morphNames.clear();
                    for (int mi : morphList)
                        morphNames.push_back(model.morphName(mi));
                    morphCStrs.clear();
                    for (auto& n : morphNames)
                        morphCStrs.push_back(n.c_str());
                }
                int sel = 0;
                for (size_t i = 0; i < morphList.size(); ++i) {
                    if (morphList[i] == morphIndex) {
                        sel = (int)i;
                        break;
                    }
                }
                if (ImGui::Combo("Morph", &sel, morphCStrs.data(), (int)morphCStrs.size())) {
                    morphIndex = morphList[sel];
                    MMD_INFO("MORPH", "select [%d/%d] %s", sel, (int)morphList.size(),
                             morphNames[sel].c_str());
                    morphWeight = model.savedMorphWeight(morphNames[sel]);
                    model.setMorphWeight(morphNames[sel], morphWeight);
                }

                // Weight slider + < > buttons
                if (morphIndex >= 0) {
                    float prev = morphWeight;
                    ImGui::SliderFloat("Weight", &morphWeight, 0.0f, 1.0f, "%.2f");
                    if (morphWeight != prev) {
                        auto name = model.morphName(morphIndex);
                        model.setMorphWeight(name, morphWeight);
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("<")) {
                        auto it = std::find(morphList.begin(), morphList.end(), morphIndex);
                        int idx = it != morphList.end() ? (int)(it - morphList.begin()) : 0;
                        idx = (idx - 1 + (int)morphList.size()) % (int)morphList.size();
                        morphIndex = morphList[idx];
                        std::string name = model.morphName(morphIndex);
                        MMD_INFO("MORPH", "select [%d/%d] %s", idx, (int)morphList.size(),
                                 name.c_str());
                        morphWeight = model.savedMorphWeight(name);
                        model.setMorphWeight(name, morphWeight);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(">")) {
                        auto it = std::find(morphList.begin(), morphList.end(), morphIndex);
                        int idx = it != morphList.end() ? (int)(it - morphList.begin()) : 0;
                        idx = (idx + 1) % (int)morphList.size();
                        morphIndex = morphList[idx];
                        std::string name = model.morphName(morphIndex);
                        MMD_INFO("MORPH", "select [%d/%d] %s", idx, (int)morphList.size(),
                                 name.c_str());
                        morphWeight = model.savedMorphWeight(name);
                        model.setMorphWeight(name, morphWeight);
                    }
                }
            }
        }

        // ── Light ──
        if (ImGui::CollapsingHeader("Light")) {
            ImGui::SliderFloat("Dir X", &pipe.lightDir[0], -1.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Dir Y", &pipe.lightDir[1], -1.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Dir Z", &pipe.lightDir[2], -1.0f, 1.0f, "%.2f");
        }

        // ── Camera ──
        if (ImGui::CollapsingHeader("Camera")) {
            const char* modeName = cam.mode() == CameraMode::FPS ? "FPS" : "Orbit";
            ImGui::Text("Mode: %s (M)", modeName);
            if (ImGui::Button("Toggle Mode (M)"))
                cam.toggleMode();
            ImGui::SameLine();
            if (ImGui::Button("Reset (R)"))
                cam.reset();
            Vec3 eye;
            cam.getEyePosition(eye.x, eye.y, eye.z);
            ImGui::Text("Eye: %.1f, %.1f, %.1f", eye.x, eye.y, eye.z);
        }

        ImGui::End();

        auto* win = app.glfwWindow();
        if (cam.mode() == CameraMode::FPS) {
            cam.update(dt, glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS,
                       glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS,
                       glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS,
                       glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS,
                       glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS,
                       glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS);
        }
        model.update(dt);
    };

    // Render
    app.onRender = [&]() {
        pipe.render(app.width(), app.height());
        imgui.endFrame();
    };

    app.run();
    imgui.shutdown();
    mmd::dispose();  // Pipeline → ShaderManager → GPU device, in order
    return 0;
}
