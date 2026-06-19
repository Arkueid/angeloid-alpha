#pragma once

#include <array>

// ──── PassParams — data bundles passed to Renderable pass callbacks ────
//
//   Bundling parameters into structs means adding a field doesn't force
//   every Renderable implementation to update its signature.

struct MainPassParams {
    std::array<float, 16> proj;
    std::array<float, 16> view;
    std::array<float, 16> model;
    std::array<float, 16> lightViewProj;
    std::array<float, 3>  lightDir;
    bool hasShadow;
};

struct ShadowPassParams {
    std::array<float, 16> lightViewProj;
    std::array<float, 16> model;
};

struct DebugPassParams {
    std::array<float, 16> proj;
    std::array<float, 16> view;
    std::array<float, 16> model;
};
