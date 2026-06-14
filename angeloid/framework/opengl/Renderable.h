#pragma once

#include "core/math/VecMath.h"
#include <array>

// ──── Renderable — renderable interface ────
//
//   Pipeline traverses registered items per pass: shadow → main → debug.
//   Pass parameters bundled into structs — adding a field doesn't force
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

class Renderable {
public:
    virtual ~Renderable() = default;
    virtual const char* name() const = 0;

    bool visible = true;

    virtual bool castShadow() const { return false; }

    // World-space AABB for shadow projection. Returns false if no bounds available.
    virtual bool shadowBounds(Vec3& outMin, Vec3& outMax) const { return false; }

    virtual void onShadowPass(const ShadowPassParams&) {}
    virtual void onMainPass(const MainPassParams&) {}
    virtual void onDebugPass(const DebugPassParams&) {}
};
