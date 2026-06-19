#pragma once

#include "core/math/VecMath.h"
#include "framework/PassParams.h"

// ──── Renderable — renderable interface ────
//
//   Pipeline traverses registered items per pass: shadow → main → debug.
//   Pass parameters bundled into structs — adding a field doesn't force
//   every Renderable implementation to update its signature.

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
