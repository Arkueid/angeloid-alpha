#pragma once

#include <array>

// ──── Renderable — renderable interface ────
//
//   Pipeline traverses registered items per pass: shadow → main → debug.
//   Each item fetches its own shader from ShaderManager.
//   castShadow() selects items for the shadow pass.
//   Items are drawn in registration order (addRenderable sequence).

class Renderable {
public:
    virtual ~Renderable() = default;
    virtual const char* name() const = 0;

    bool visible = true;

    virtual bool castShadow() const { return false; }

    virtual void onShadowPass(const std::array<float, 16>& lightViewProj,
                              const std::array<float, 16>& model) {}

    virtual void onMainPass(const std::array<float, 16>& proj,
                            const std::array<float, 16>& view,
                            const std::array<float, 16>& model,
                            const std::array<float, 16>& lightViewProj,
                            bool hasShadow) {}

    virtual void onDebugPass(const std::array<float, 16>& proj,
                             const std::array<float, 16>& view,
                             const std::array<float, 16>& model) {}
};
