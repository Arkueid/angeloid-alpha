#include "core/Application.h"
#include "core/Camera.h"
#include "render/ModelRenderer.h"
#include "anim/MorphController.h"
#include "pmx/PmxReader.h"
#include "render/ShaderManager.h"
#include "anim/BoneSkinning.h"
#include "anim/PhysicsWorld.h"
#include "anim/VmdPlayer.h"
#include "render/PhysicsDebug.h"
#include "render/WorldAxis.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace fs = std::filesystem;

static void printHelp()
{
    std::cout << "\nFPS Camera Controls:\n"
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
        "  K key: Toggle GPU skinning\n"
        "  P key: Toggle VPD pose\n"
        "  R key: Reset camera to default position\n"
        "  I key: Toggle idle animation\n"
        "  M key: Toggle morph mode\n"
        "  < / > keys: Switch between morphs\n"
        "  Up/Down keys: Adjust morph weight\n"
        "VMD Animation Controls:\n"
        "  Space: Play/Pause VMD animation\n"
        "  L key: Toggle VMD loop\n"
        "  [ / ] keys: Step backward/forward 30 frames\n"
        << std::endl;
}

struct ModelEntry { const char* pmx; const char* texDir; };
static const std::unordered_map<std::string, ModelEntry> MODELS = {
    {"ikaros-origin",   {"resources/models/ikaros-origin/Ikaros.pmx", "resources/models/ikaros-origin"}},
    {"ikaros-uniform",  {"resources/models/ikaros-uniform/Ikaros.pmx", "resources/models/ikaros-uniform"}},
    {"安比",             {"resources/models/安比/安比.pmx", "resources/models/安比"}},
    {"刀",              {"resources/models/安比/刀.pmx", "resources/models/安比"}},
    {"chloe",           {"resources/models/Chloe_Uniform1_0.9/Chloe_Uniform1_0.9.pmx", "resources/models/Chloe_Uniform1_0.9/textures"}},
    {"aqua-swimwear",   {"resources/models/Aqua_Swimwear_1.0/Aqua_Swimwear_1.0.pmx", "resources/models/Aqua_Swimwear_1.0/textures"}},
    {"marine-swimwear", {"resources/models/Marine_Swmwear_1.01/Marine_Swmwear_1.01.pmx", "resources/models/Marine_Swmwear_1.01/textures"}},
    {"aqua-basebody",   {"resources/models/Aqua_BaseBody_R15_0.9/Aqua_BaseBody_R15_0.9.pmx", "resources/models/Aqua_BaseBody_R15_0.9/textures"}},
    {"aqua-sailor",     {"resources/models/Aqua_Sailor_0.8/Aqua_Sailor_0.8.pmx", "resources/models/Aqua_Sailor_0.8/textures"}},
    {"brujas",          {"resources/models/Brujas/Brujas.pmx", "resources/models/Brujas"}},
    {"lamy-swimwear",   {"resources/models/Lamy_Swimwear_1.0/Lamy_Swimwear_1.0.pmx", "resources/models/Lamy_Swimwear_1.0/textures"}},
    {"lulum",           {"resources/models/lulum/lulum_1.0.pmx", "resources/models/lulum/textures"}},
    {"marine-jk1",      {"resources/models/Marine_JK1_Set_1.01/Marine_JK1_1.0.pmx", "resources/models/Marine_JK1_Set_1.01/textures"}},
    {"marine-jk1-hi",   {"resources/models/Marine_JK1_Set_1.01/Marine_JK1_Hi_1.0.pmx", "resources/models/Marine_JK1_Set_1.01/textures"}},
    {"rurudo-lion",     {"resources/models/RurudoLion_1.0/RurudoLion_1.0.pmx", "resources/models/RurudoLion_1.0/textures"}},
    {"rurudo-lion-hi",  {"resources/models/RurudoLion_1.0/RurudoLion_Hi_1.0.pmx", "resources/models/RurudoLion_1.0/textures"}},
    {"卢西娅",           {"resources/models/卢西娅/卢西娅.pmx", "resources/models/卢西娅/textures"}},
    {"卢西娅-摘帽",      {"resources/models/卢西娅/卢西娅_摘帽.pmx", "resources/models/卢西娅/textures"}},
    {"卢西娅-武器1",     {"resources/models/卢西娅/武器1.pmx", "resources/models/卢西娅/textures"}},
    {"卢西娅-武器2",     {"resources/models/卢西娅/武器2.pmx", "resources/models/卢西娅/textures"}},
};

int main(int argc, char* argv[])
{
#ifdef _WIN32
    std::system("chcp 65001 > nul");
    // Convert argv from system locale encoding (ACP) to UTF-8
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
    for (int i = 0; i < argc; ++i) u8argv[i] = u8args[i].data();
    argv = u8argv.data();
#endif
    std::cout << "MMD PMX Viewer (C++)" << std::endl;

    std::string modelName = "ikaros-uniform";
    std::vector<fs::path> vmdPaths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--model" || arg == "-m") && i + 1 < argc) {
            modelName = argv[++i];
        } else if (arg == "--vmd" || arg == "-v") {
            while (i + 1 < argc && argv[i + 1][0] != '-')
                vmdPaths.push_back(fs::u8path(argv[++i]));
        } else if (arg[0] != '-') {
            modelName = arg;
        }
    }

    // Resolve model name -> path
    fs::path pmxPath;
    fs::path texDir;
    auto it = MODELS.find(modelName);
    if (it != MODELS.end()) {
        pmxPath = fs::u8path(it->second.pmx);
        texDir = fs::u8path(it->second.texDir);
    } else {
        pmxPath = fs::u8path(modelName);
        texDir = fs::u8path(modelName).parent_path();
    }

    // VPD: auto-load like Python
    fs::path vpdPath = fs::u8path("resources/vpd/自然站姿.vpd");

    std::cout << "Loading: " << pmxPath.string() << std::endl;
    if (!fs::exists(pmxPath)) {
        std::cerr << "Model not found: " << pmxPath.string() << std::endl;
        return 1;
    }
    PmxModel model = PmxReader::load(pmxPath);

    std::cout << "Model: " << model.name << " (" << model.english_name << ")" << std::endl;
    std::cout << "Vertices: " << model.vertexCount() << ", Faces: " << model.faceCount()
              << ", Bones: " << model.boneCount() << std::endl;

    fs::path projRoot = fs::weakly_canonical(fs::path(MMD_PROJECT_ROOT));

    Application app(1280, 720, "MMD PMX Viewer - " + model.name);

    ShaderManager shaders(projRoot / "resources/shaders");
    WorldAxis worldAxis;
    Camera camera;

    PhysicsDebug physicsDebug;
    physicsDebug.showRigidBody = false;
    physicsDebug.showJoint = false;

    PhysicsWorld physicsWorld;

    ModelRenderer renderer;
    if (texDir.is_relative()) texDir = projRoot / texDir;
    renderer.loadModel(model, texDir, projRoot / "resources/toon");

    physicsDebug.build(model, renderer.modelScale());

    physicsWorld.build(model, renderer.modelScale());
    physicsWorld.enabled = false;

    // VPD + skinning
    std::unordered_map<std::string, VpdPose> vpdPoses;
    bool vpdPoseApplied = false;
    if (!vpdPath.empty() && vpdPath.is_relative()) vpdPath = projRoot / vpdPath;
    if (!vpdPath.empty() && fs::exists(vpdPath)) {
        vpdPoses = VpdLoader::load(vpdPath);
        vpdPoseApplied = true;
        std::cout << "VPD: " << vpdPoses.size() << " poses" << std::endl;
        renderer.setupSkinning(model, vpdPath);
    } else if (!vpdPath.empty()) {
        std::cout << "VPD not found: " << vpdPath.string() << std::endl;
        renderer.setupSkinning(model);
    } else {
        renderer.setupSkinning(model);
    }
    renderer.useSkinning = true;
    physicsDebug.useBoneMatrices = false;

    MorphController morphCtl;
    morphCtl.setModel(model, renderer.morphVbo(), renderer.uvMorphVbo(), renderer.modelScale());
    bool showMorph = true;
    bool idleEnabled = true;

    // Morph cycling state
    int activeMorphIndex = -1;
    float morphWeightValue = 0.0f;
    std::vector<int> availableMorphs;
    for (const auto& m : model.morphs) {
        if (m.morph_type == MORPH_TYPE_VERTEX || m.morph_type == MORPH_TYPE_GROUP ||
            m.morph_type == MORPH_TYPE_MATERIAL || m.morph_type == MORPH_TYPE_UV ||
            m.morph_type == MORPH_TYPE_BONE)
            availableMorphs.push_back(m.index);
    }

    // VMD animation (multi-layer mixer)
    std::unique_ptr<VmdMixer> vmdMixer;
    for (auto& vp : vmdPaths) {
        if (vp.is_relative()) vp = projRoot / vp;
        if (!fs::exists(vp)) {
            std::cerr << "VMD not found: " << vp.string() << std::endl;
            return 1;
        }
        auto vmdAnim = VmdAnimation::load(vp);
        if (!vmdMixer) vmdMixer = std::make_unique<VmdMixer>();
        std::cout << "VMD: " << vmdAnim.modelName << " (max frame: "
                  << vmdAnim.maxFrame << ")" << std::endl;
        vmdMixer->addVmd(std::move(vmdAnim));
    }
    if (vmdMixer) vmdMixer->play();

    printHelp();

    // Input
    app.onMouseButton = [&camera](int b, int a, int m) { camera.onMouseButton(b, a, m); };
    app.onCursorPos  = [&camera](double x, double y) { camera.onCursorPos(x, y); };
    app.onScroll     = [&camera](double xo, double yo) { camera.onScroll(xo, yo); };
    app.onKey = [&](int key, int sc, int act, int mods) {
        (void)sc; (void)mods;
        if (key == GLFW_KEY_ESCAPE && act == GLFW_PRESS) {
            glfwSetWindowShouldClose(app.window(), GLFW_TRUE);
        }
        if (key == GLFW_KEY_X && act == GLFW_PRESS) {
            worldAxis.showAxis = !worldAxis.showAxis;
            std::cout << "World axis: " << (worldAxis.showAxis ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_G && act == GLFW_PRESS) {
            worldAxis.showGrid = !worldAxis.showGrid;
            std::cout << "Ground grid: " << (worldAxis.showGrid ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_B && act == GLFW_PRESS) {
            physicsDebug.showRigidBody = !physicsDebug.showRigidBody;
            std::cout << "Rigidbody: " << (physicsDebug.showRigidBody ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_F && act == GLFW_PRESS) {
            if (physicsWorld.enabled)
                physicsWorld.debugDump();
            else
                std::cout << "Enable physics (Y) first" << std::endl;
        }
        if (key == GLFW_KEY_H && act == GLFW_PRESS) {
            renderer.showModel = !renderer.showModel;
            std::cout << "Model mesh: " << (renderer.showModel ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_O && act == GLFW_PRESS) {
            renderer.showOutline = !renderer.showOutline;
            std::cout << "Outline: " << (renderer.showOutline ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_T && act == GLFW_PRESS) {
            renderer.showToon = !renderer.showToon;
            std::cout << "Toon shading: " << (renderer.showToon ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_K && act == GLFW_PRESS) {
            renderer.useSkinning = !renderer.useSkinning;
            if (renderer.useSkinning) {
                if (vpdPoseApplied)
                    renderer.updateBoneTexture(model, vpdPoses, {});
                else
                    renderer.updateBoneTexture(model, {}, {});
            }
            std::cout << "Skinned rendering: " << (renderer.useSkinning ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_Y && act == GLFW_PRESS) {
            physicsWorld.enabled = !physicsWorld.enabled;
            std::cout << "Physics: " << (physicsWorld.enabled ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_P && act == GLFW_PRESS) {
            if (!vpdPoses.empty()) {
                vpdPoseApplied = !vpdPoseApplied;
                if (vpdPoseApplied) {
                    if (vmdMixer) {
                        std::unordered_map<std::string, std::pair<std::array<float,3>, std::array<float,4>>> vmdT;
                        for (const auto& bone : model.bones) {
                            std::array<float,3> pos; std::array<float,4> rot;
                            if (vmdMixer->getBoneTransform(bone.name, pos, rot))
                                vmdT[bone.name] = {pos, rot};
                        }
                        auto& bm = morphCtl.boneMorphs();
                        renderer.updateBoneTexture(model, vpdPoses, vmdT, bm.empty() ? nullptr : &bm);
                    } else {
                        auto& bm = morphCtl.boneMorphs();
                        renderer.updateBoneTexture(model, vpdPoses, {}, bm.empty() ? nullptr : &bm);
                    }
                } else {
                    auto& bm = morphCtl.boneMorphs();
                    renderer.updateBoneTexture(model, {}, {}, bm.empty() ? nullptr : &bm);
                }
                std::cout << "VPD pose: " << (vpdPoseApplied ? "ON" : "OFF") << std::endl;
            } else {
                std::cout << "No VPD pose loaded" << std::endl;
            }
        }
        if (key == GLFW_KEY_R && act == GLFW_PRESS) {
            camera.reset();
            std::cout << "Camera reset to default position" << std::endl;
        }
        if (key == GLFW_KEY_I && act == GLFW_PRESS) {
            idleEnabled = !idleEnabled;
            if (!idleEnabled) morphCtl.clearMorphs();
            std::cout << "Idle animation: " << (idleEnabled ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_M && act == GLFW_PRESS) {
            showMorph = !showMorph;
            if (showMorph && !availableMorphs.empty()) {
                activeMorphIndex = availableMorphs[0];
                morphWeightValue = 0.0f;
                morphCtl.clearMorphs();
                const auto& m = model.morphs[activeMorphIndex];
                morphCtl.setMorphWeight(m.name, morphWeightValue);
                std::cout << "Morph mode: ON (showing morph: " << m.name << ")" << std::endl;
            } else if (showMorph) {
                std::cout << "Morph mode: ON (no available morphs found)" << std::endl;
            } else {
                morphCtl.clearMorphs();
                std::cout << "Morph mode: OFF" << std::endl;
            }
        }
        if (key == GLFW_KEY_COMMA && (act == GLFW_PRESS || act == GLFW_REPEAT)) {
            if (showMorph && !availableMorphs.empty()) {
                auto it = std::find(availableMorphs.begin(), availableMorphs.end(), activeMorphIndex);
                int idx = (it != availableMorphs.end()) ? (int)(it - availableMorphs.begin()) : 0;
                idx = (idx - 1 + (int)availableMorphs.size()) % (int)availableMorphs.size();
                activeMorphIndex = availableMorphs[idx];
                morphCtl.clearMorphs();
                const auto& m = model.morphs[activeMorphIndex];
                morphCtl.setMorphWeight(m.name, morphWeightValue);
                std::cout << "Active morph: " << m.name << " (weight=" << morphWeightValue << ")" << std::endl;
            }
        }
        if (key == GLFW_KEY_PERIOD && (act == GLFW_PRESS || act == GLFW_REPEAT)) {
            if (showMorph && !availableMorphs.empty()) {
                auto it = std::find(availableMorphs.begin(), availableMorphs.end(), activeMorphIndex);
                int idx = (it != availableMorphs.end()) ? (int)(it - availableMorphs.begin()) : 0;
                idx = (idx + 1) % (int)availableMorphs.size();
                activeMorphIndex = availableMorphs[idx];
                morphCtl.clearMorphs();
                const auto& m = model.morphs[activeMorphIndex];
                morphCtl.setMorphWeight(m.name, morphWeightValue);
                std::cout << "Active morph: " << m.name << " (weight=" << morphWeightValue << ")" << std::endl;
            }
        }
        if (key == GLFW_KEY_UP && (act == GLFW_PRESS || act == GLFW_REPEAT)) {
            if (showMorph) {
                morphWeightValue = std::min(1.0f, morphWeightValue + 0.1f);
                if (activeMorphIndex >= 0 && activeMorphIndex < model.morphCount()) {
                    morphCtl.setMorphWeight(model.morphs[activeMorphIndex].name, morphWeightValue);
                    std::cout << "Morph weight: " << morphWeightValue << std::endl;
                }
            }
        }
        if (key == GLFW_KEY_DOWN && (act == GLFW_PRESS || act == GLFW_REPEAT)) {
            if (showMorph) {
                morphWeightValue = std::max(0.0f, morphWeightValue - 0.1f);
                if (activeMorphIndex >= 0 && activeMorphIndex < model.morphCount()) {
                    morphCtl.setMorphWeight(model.morphs[activeMorphIndex].name, morphWeightValue);
                    std::cout << "Morph weight: " << morphWeightValue << std::endl;
                }
            }
        }
        if (key == GLFW_KEY_SPACE && act == GLFW_PRESS) {
            if (vmdMixer) {
                if (vmdMixer->playing()) vmdMixer->pause();
                else vmdMixer->play();
            } else {
                std::cout << "No VMD animation loaded" << std::endl;
            }
        }
        if (key == GLFW_KEY_L && act == GLFW_PRESS) {
            if (vmdMixer) {
                vmdMixer->setLoop(!vmdMixer->loop());
                std::cout << "VMD loop: " << (vmdMixer->loop() ? "ON" : "OFF") << std::endl;
            }
        }
        if (key == GLFW_KEY_LEFT_BRACKET && (act == GLFW_PRESS || act == GLFW_REPEAT)) {
            if (vmdMixer) {
                vmdMixer->setFrame(std::max(0.0f, vmdMixer->currentFrame() - 30));
                std::cout << "VMD frame: " << vmdMixer->currentFrame()
                          << "/" << vmdMixer->maxFrame() << std::endl;
            }
        }
        if (key == GLFW_KEY_RIGHT_BRACKET && (act == GLFW_PRESS || act == GLFW_REPEAT)) {
            if (vmdMixer) {
                vmdMixer->setFrame(std::min((float)vmdMixer->maxFrame(),
                                              vmdMixer->currentFrame() + 30));
                std::cout << "VMD frame: " << vmdMixer->currentFrame()
                          << "/" << vmdMixer->maxFrame() << std::endl;
            }
        }
    };

    // FPS tracking
    int fpsFrameCount = 0;
    float fpsElapsed = 0;

    // Update
    app.onUpdate = [&](float dt) {
        camera.update(glfwGetCurrentContext(), dt);
        fpsFrameCount++;
        fpsElapsed += dt;
        if (fpsElapsed >= 0.5f) {
            float fps = (float)fpsFrameCount / fpsElapsed;
            app.setTitle("MMD PMX Viewer - " + model.name + " [" + std::to_string((int)fps) + " FPS]");
            fpsFrameCount = 0;
            fpsElapsed = 0;
        }

        if (vmdMixer) {
            vmdMixer->update(dt);
            std::unordered_map<std::string, std::pair<std::array<float,3>, std::array<float,4>>> vmdTransforms;
            for (const auto& bone : model.bones) {
                std::array<float,3> pos; std::array<float,4> rot;
                if (vmdMixer->getBoneTransform(bone.name, pos, rot))
                    vmdTransforms[bone.name] = {pos, rot};
            }
            if (!vmdTransforms.empty())
                renderer.updateBoneTexture(model, vpdPoses, vmdTransforms);
            // Apply VMD morph weights (batch)
            std::unordered_map<std::string, float> vmdMorphs;
            for (const auto& m : model.morphs) {
                float w = vmdMixer->getMorphWeight(m.name);
                if (w != 0) vmdMorphs[m.name] = w;
            }
            if (!vmdMorphs.empty())
                morphCtl.setMorphWeights(vmdMorphs);
        } else {
            auto& boneMorphs = morphCtl.boneMorphs();
            renderer.updateBoneTexture(model, vpdPoses, {}, boneMorphs.empty() ? nullptr : &boneMorphs);
        }

        // Update mode 0 rigid bodies from bone animation, then step physics
        if (physicsWorld.enabled) {
            auto poseWorld = BoneSkinning::computePoseWorldMatrices(model, vpdPoses);
            physicsWorld.updateMode0Bodies(poseWorld);
            physicsWorld.step(dt);
        }
        // (VMD bone→rigid body not yet implemented; VPD case works)
    };

    // Render
    float idleTime = 0;
    app.onRender = [&]() {
        glFrontFace(GL_CW);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto proj = Camera::projectionMatrix(app.width(), app.height());
        auto view = camera.viewMatrix();

        if (auto* s = shaders.get("axis")) {
            worldAxis.render(*s, proj, view);
        }

        // Idle animation
        bool idleActive = idleEnabled && (!vmdMixer || !vmdMixer->playing());
        if (idleActive) idleTime += app.deltaTime();

        if (idleActive) {
            float blinkPhase = fmodf(idleTime, 4.0f);
            float w = 0;
            if (blinkPhase < 0.15f) {
                float t = blinkPhase / 0.15f;
                w = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
            }
            std::unordered_map<std::string, float> bm;
            for (auto& nm : {"blink", "blink_l", "blink_r", "まばたき", "まぶたき", "ウィンク", "ｳｨﾝｸ"})
                bm[nm] = w;
            // Merge blink morphs into existing weights (don't replace manual selection)
            for (auto& [name, wt] : bm)
                morphCtl.morphWeights()[name] = wt;
            morphCtl.updateMorphOffsets();
        }

        // Sync morph material overrides
        renderer.clearMaterialOverrides();
        for (size_t i = 0; i < model.materials.size(); ++i) {
            if (auto* ov = morphCtl.getMaterialOverride((int)i)) {
                renderer.setMaterialOverride((int)i, *ov);
            }
        }

        if (renderer.useSkinning) {
            bool useMorph = showMorph && morphCtl.hasActiveMorphs();
            if (auto* s = shaders.get(useMorph ? "morph_outline" : "outline_skinned"))
                useMorph ? renderer.renderMorphOutlinePass(*s, proj, view) : renderer.renderSkinnedOutlinePass(*s, proj, view);
            auto* sn = useMorph ? (renderer.showToon ? "morph" : "morph_notoon")
                                : (renderer.showToon ? "skinned" : "skinned_notoon");
            if (auto* s = shaders.get(sn)) {
                if (renderer.showToon) {
                    s->use();
                    s->setVec3("camera_pos", camera.x, camera.y, camera.z);
                    s->setFloat("shadow_thresh", 0.0f);
                    s->setFloat("rim_power", 4.0f);
                    s->setVec3("rim_color", 1.0f, 1.0f, 1.0f);
                    s->setInt("gradient_map", 2);
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, shaders.gradientTexture()->id);
                }
                useMorph ? renderer.renderMorphMainPass(*s, proj, view) : renderer.renderSkinnedMainPass(*s, proj, view);
            }
        } else {
            if (auto* s = shaders.get("outline"))
                renderer.renderOutlinePass(*s, proj, view);
            auto* sn = renderer.showToon ? "toon" : "main";
            if (auto* s = shaders.get(sn)) {
                if (renderer.showToon) {
                    s->use();
                    s->setVec3("camera_pos", camera.x, camera.y, camera.z);
                    s->setFloat("shadow_thresh", 0.0f);
                    s->setFloat("rim_power", 4.0f);
                    s->setVec3("rim_color", 1.0f, 1.0f, 1.0f);
                    s->setInt("gradient_map", 1);
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, shaders.gradientTexture()->id);
                }
                renderer.renderMainPass(*s, proj, view);
            }
        }

        // Physics debug
        if (auto* s = shaders.get("rigidbody")) {
            if (physicsWorld.enabled && physicsDebug.showRigidBody)
                physicsDebug.updateFromPhysics(physicsWorld);
            glLineWidth(2.0f);
            physicsDebug.render(*s, proj, view);
            glLineWidth(1.0f);
        }
    };

    app.run();
    glFinish();

    return 0;
}
