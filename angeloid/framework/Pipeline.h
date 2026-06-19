#pragma once

#include "core/math/VecMath.h"
#include "framework/gpu/IGpuRenderTarget.h"

#include <array>
#include <memory>
#include <vector>

class Renderable;

// ──── Pipeline — render orchestrator ────
//
//   Manages: pass ordering, renderable list, shadow map, GPU state.
//   Shaders live in ShaderManager (resource layer), queried by Renderables.

class Pipeline {
public:
    static Pipeline& instance();

    void init();
    void clear();

    void addRenderable(Renderable* item);
    void removeRenderable(Renderable* item);

    // ── Per-frame: resize + light + execute in one call ──
    void render(int screenW, int screenH);

    bool showSelfShadow = true;
    bool showGroundShadow = true;
    float lightDir[3] = {0.3f, 0.8f, 0.5f};

private:
    Pipeline() = default;

    void renderShadowPass(const std::array<float, 16>& lightViewProj);
    void resizeViewport(int w, int h);
    void computeLightMatrix(const float* lightDir,
                            const float* sceneMin, const float* sceneMax);
    void execute(const std::array<float, 16>& proj,
                 const std::array<float, 16>& view);
    bool collectShadowBounds(Vec3& outMin, Vec3& outMax) const;

    std::vector<Renderable*> mItems;

    std::unique_ptr<Gpu::IGpuRenderTarget> mShadowMap;
    std::array<float, 16> mLightViewProj{};
    int mViewportW = 0, mViewportH = 0;
};
