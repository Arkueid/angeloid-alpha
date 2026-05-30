#include "Model.h"
#include "debug/WorldAxis.h"
#include "render/opengl/gpu/Shader.h"
#include "window/GlfwWindow.h"

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
                 "  < / > keys: Switch between morphs\n"
                 "  Up/Down keys: Adjust morph weight\n"
                 "VMD Animation Controls:\n"
                 "  Space: Play/Pause VMD animation\n"
                 "  L key: Toggle VMD loop\n"
                 "  [ / ] keys: Step backward/forward 30 frames\n"
              << std::endl;
}

static const std::unordered_map<std::string, const char*> MODELS = {
    {"ikaros-origin", "resources/models/ikaros-origin/Ikaros.pmx"},
    {"ikaros-uniform", "resources/models/ikaros-uniform/Ikaros.pmx"},
    {"安比", "resources/models/安比/安比.pmx"},
    {"刀", "resources/models/安比/刀.pmx"},
    {"chloe", "resources/models/Chloe_Uniform1_0.9/Chloe_Uniform1_0.9.pmx"},
    {"aqua-swimwear", "resources/models/Aqua_Swimwear_1.0/Aqua_Swimwear_1.0.pmx"},
    {"marine-swimwear", "resources/models/Marine_Swmwear_1.01/Marine_Swmwear_1.01.pmx"},
    {"aqua-basebody", "resources/models/Aqua_BaseBody_R15_0.9/Aqua_BaseBody_R15_0.9.pmx"},
    {"aqua-sailor", "resources/models/Aqua_Sailor_0.8/Aqua_Sailor_0.8.pmx"},
    {"brujas", "resources/models/Brujas/Brujas.pmx"},
    {"lamy-swimwear", "resources/models/Lamy_Swimwear_1.0/Lamy_Swimwear_1.0.pmx"},
    {"lulum", "resources/models/lulum/lulum_1.0.pmx"},
    {"marine-jk1", "resources/models/Marine_JK1_Set_1.01/Marine_JK1_1.0.pmx"},
    {"marine-jk1-hi", "resources/models/Marine_JK1_Set_1.01/Marine_JK1_Hi_1.0.pmx"},
    {"rurudo-lion", "resources/models/RurudoLion_1.0/RurudoLion_1.0.pmx"},
    {"rurudo-lion-hi", "resources/models/RurudoLion_1.0/RurudoLion_Hi_1.0.pmx"},
    {"卢西娅", "resources/models/卢西娅/卢西娅.pmx"},
    {"卢西娅-摘帽", "resources/models/卢西娅/卢西娅_摘帽.pmx"},
    {"卢西娅-武器1", "resources/models/卢西娅/武器1.pmx"},
    {"卢西娅-武器2", "resources/models/卢西娅/武器2.pmx"},
};

int main(int argc, char* argv[]) {
#ifdef _WIN32
    std::system("chcp 65001 > nul");
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

    fs::path pmxPath;
    auto it = MODELS.find(modelName);
    if (it != MODELS.end())
        pmxPath = fs::u8path(it->second);
    else
        pmxPath = fs::u8path(modelName);
    fs::path vpdPath = fs::u8path("resources/vpd/自然站姿.vpd");
    fs::path projRoot = fs::weakly_canonical(fs::path(MMD_PROJECT_ROOT));
    if (pmxPath.is_relative())
        pmxPath = projRoot / pmxPath;
    if (vpdPath.is_relative())
        vpdPath = projRoot / vpdPath;
    fs::path texDir = pmxPath.parent_path();

    // --- Window ---
    GlfwWindow app(1280, 720, "MMD PMX Viewer");

    // --- Load model ---
    mmd::Model model;
    model.load(pmxPath, texDir, projRoot / "resources/toon", projRoot / "resources/shaders");
    app.setTitle("MMD PMX Viewer - " + model.modelName());
    if (!vpdPath.empty())
        model.loadVpd(vpdPath);

    // --- VMD ---
    for (auto& vp : vmdPaths) {
        if (vp.is_relative())
            vp = projRoot / vp;
        model.loadVmd(vp);
    }

    WorldAxis worldAxis;
    auto axisVert = Gpu::ShaderProgram::readFile(projRoot / "resources/shaders/axis.vert");
    auto axisFrag = Gpu::ShaderProgram::readFile(projRoot / "resources/shaders/axis.frag");
    Gpu::ShaderProgram axisShader(axisVert, axisFrag);

    printHelp();

    // Morph state
    int morphIndex = -1;
    float morphWeight = 0.0f;
    std::unordered_map<std::string, float> savedWeights;
    auto morphList = model.interactableMorphs();

    // Input
    auto& cam = Camera::instance();
    app.onMouseButton = [&cam](int b, int a, int) {
        if (b == GLFW_MOUSE_BUTTON_LEFT)
            cam.onMouseButton(a == GLFW_PRESS);
    };
    app.onCursorPos = [&cam](double x, double y) {
        cam.onCursorPos(x, y);
    };
    app.onScroll = [&cam](double, double yo) {
        cam.onScroll(yo);
    };
    app.onKey = [&](int key, int, int act, int) {
        if (key == GLFW_KEY_ESCAPE && act == GLFW_PRESS)
            app.close();
        if (key == GLFW_KEY_X && act == GLFW_PRESS)
            worldAxis.showAxis = !worldAxis.showAxis;
        if (key == GLFW_KEY_G && act == GLFW_PRESS)
            worldAxis.showGrid = !worldAxis.showGrid;
        if (key == GLFW_KEY_B && act == GLFW_PRESS) {
            static bool showDbg = false;
            showDbg = !showDbg;
            model.showRigidBodies(showDbg);
        }
        if (key == GLFW_KEY_H && act == GLFW_PRESS)
            model.showModel(!model.showModel());
        if (key == GLFW_KEY_O && act == GLFW_PRESS)
            model.showOutline(!model.showOutline());
        if (key == GLFW_KEY_T && act == GLFW_PRESS)
            model.showToon(!model.showToon());
        if (key == GLFW_KEY_P && act == GLFW_PRESS) {
            model.applyVpd(!model.vpdApplied());
            std::cout << "VPD pose: " << (model.vpdApplied() ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_K && act == GLFW_PRESS) {
            model.setSkinning(!model.isSkinned());
            std::cout << "GPU skinning: " << (model.isSkinned() ? "ON" : "OFF") << std::endl;
        }
        if (key == GLFW_KEY_Y && act == GLFW_PRESS) {
            model.enablePhysics(!model.physicsEnabled());
            std::cout << "Physics: " << (model.physicsEnabled() ? "ON" : "OFF") << std::endl;
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
                morphWeight = savedWeights.count(name) ? savedWeights[name] : 0.0f;
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
                morphWeight = savedWeights.count(name) ? savedWeights[name] : 0.0f;
                model.setMorphWeight(name, morphWeight);
            }
        }
        if (key == GLFW_KEY_UP && act != GLFW_RELEASE) {
            if (morphIndex >= 0 && morphIndex < model.morphCount()) {
                morphWeight = std::min(1.0f, morphWeight + 0.1f);
                auto name = model.morphName(morphIndex);
                savedWeights[name] = morphWeight;
                model.setMorphWeight(name, morphWeight);
            }
        }
        if (key == GLFW_KEY_DOWN && act != GLFW_RELEASE) {
            if (morphIndex >= 0 && morphIndex < model.morphCount()) {
                morphWeight = std::max(0.0f, morphWeight - 0.1f);
                auto name = model.morphName(morphIndex);
                savedWeights[name] = morphWeight;
                model.setMorphWeight(name, morphWeight);
            }
        }
        if (key == GLFW_KEY_SPACE && act == GLFW_PRESS) {
            if (model.hasVmd())
                model.vmdPlaying() ? model.vmdPause() : model.vmdPlay();
        }
        if (key == GLFW_KEY_L && act == GLFW_PRESS) {
            if (model.hasVmd())
                model.setVmdLoop(!model.vmdLoop());
        }
        if (key == GLFW_KEY_LEFT_BRACKET && act != GLFW_RELEASE) {
            if (model.hasVmd())
                model.setVmdFrame(std::max(0.0f, model.vmdCurrentFrame() - 30));
        }
        if (key == GLFW_KEY_RIGHT_BRACKET && act != GLFW_RELEASE) {
            if (model.hasVmd())
                model.setVmdFrame(std::min(model.vmdMaxFrame(), model.vmdCurrentFrame() + 30));
        }
    };

    // Update
    app.onUpdate = [&](float dt) {
        auto* win = app.glfwWindow();
        cam.update(
            dt, glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS,
            glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS, glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS,
            glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS, glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS,
            glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS);
        model.update(dt);

        static int fc = 0;
        static float et = 0;
        fc++;
        et += dt;
        if (et >= 0.5f) {
            app.setTitle("MMD PMX Viewer - " + model.modelName() + " [" +
                         std::to_string((int)(fc / et)) + " FPS]");
            fc = 0;
            et = 0;
        }
    };

    // Render
    app.onRender = [&]() {
        glFrontFace(GL_CW);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto proj = Camera::projectionMatrix(app.width(), app.height());
        auto view = Camera::instance().viewMatrix();
        axisShader.use();
        glLineWidth(2.0f);
        worldAxis.render(axisShader, proj, view);

        model.draw(app.width(), app.height());
    };

    app.run();
    glFinish();
    return 0;
}
