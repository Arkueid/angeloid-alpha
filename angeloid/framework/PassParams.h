#pragma once

#include <array>

class ShaderManager;

// ──── PassParams — data bundles passed to Renderable pass callbacks ────
//
//   All per-frame dependencies are explicit in the struct. No Renderable
//   should need to reach a singleton inside its pass callback.

struct MainPassParams {
    std::array<float, 16> proj;
    std::array<float, 16> view;
    std::array<float, 16> model;
    std::array<float, 16> lightViewProj;
    std::array<float, 3>  lightDir;
    std::array<float, 3>  cameraPos;
    bool hasShadow;
    ShaderManager* shaders = nullptr;
};

struct ShadowPassParams {
    std::array<float, 16> lightViewProj;
    std::array<float, 16> model;
    ShaderManager* shaders = nullptr;
};

struct DebugPassParams {
    std::array<float, 16> proj;
    std::array<float, 16> view;
    std::array<float, 16> model;
    ShaderManager* shaders = nullptr;
};
