#pragma once

#include "framework/opengl/RenderTarget.h"

#include <array>
#include <vector>

class Renderable;

// ──── Pipeline — render orchestrator ────
//
//   Manages: pass ordering, renderable list, shadow map, GL state.
//   Shaders live in ShaderManager (resource layer), queried by Renderables.

class Pipeline {
public:
    static Pipeline& instance();

    void init();
    void clear();

    void addRenderable(Renderable* item);
    void removeRenderable(Renderable* item);

    // ── Per-frame ──
    void computeLightMatrix(const float* lightDir);
    void execute(const std::array<float, 16>& proj,
                 const std::array<float, 16>& view);

    void resizeViewport(int w, int h);

private:
    Pipeline() = default;

    void renderShadowPass();

    std::vector<Renderable*> mItems;

    RenderTarget mShadowMap;
    std::array<float, 16> mLightViewProj{};
    int mViewportW = 0, mViewportH = 0;
};
