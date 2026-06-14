#include "framework/opengl/Pipeline.h"

#include <glad/glad.h>
#include <cmath>

#include "framework/opengl/Renderable.h"
#include "framework/opengl/ShaderManager.h"

static constexpr int kShadowMapSize = 4096;
static const std::array<float, 16> kIdentity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

Pipeline& Pipeline::instance() {
    static Pipeline p;
    return p;
}

void Pipeline::init() {
    mShadowMap.resize(kShadowMapSize, kShadowMapSize, false, true);
}

void Pipeline::clear() {
    mItems.clear();
}

void Pipeline::addRenderable(Renderable* item) {
    mItems.push_back(item);
}

void Pipeline::removeRenderable(Renderable* item) {
    auto it = std::find(mItems.begin(), mItems.end(), item);
    if (it != mItems.end())
        mItems.erase(it);
}

void Pipeline::resizeViewport(int w, int h) {
    mViewportW = w;
    mViewportH = h;
}

void Pipeline::computeLightMatrix(const float* lightDir) {
    // Key light from upper-front-right (classic MMD lighting).
    // Normalize direction (len ≈ 1.04, close enough; renormalize for robustness)
    float lx = lightDir[0], ly = lightDir[1], lz = lightDir[2];
    float l = std::sqrt(lx*lx + ly*ly + lz*lz);
    lx /= l; ly /= l; lz /= l;

    float dist = 15.0f;
    float lpx = lx * dist, lpy = ly * dist, lpz = lz * dist;

    float fwdX = -lx, fwdY = -ly, fwdZ = -lz;
    float wUpX = 0, wUpY = 1, wUpZ = 0;
    if (std::abs(fwdX) < 0.001f && std::abs(fwdZ) < 0.001f) {
        wUpX = 1; wUpY = 0; wUpZ = 0;
    }
    // Right = worldUp × fwd
    float rX = wUpY*fwdZ - wUpZ*fwdY;
    float rY = wUpZ*fwdX - wUpX*fwdZ;
    float rZ = wUpX*fwdY - wUpY*fwdX;
    float rl = std::sqrt(rX*rX + rY*rY + rZ*rZ);
    rX /= rl; rY /= rl; rZ /= rl;
    // Up = fwd × right
    float uX = fwdY*rZ - fwdZ*rY;
    float uY = fwdZ*rX - fwdX*rZ;
    float uZ = fwdX*rY - fwdY*rX;

    float size = 3.0f;
    float zn = dist - 2.0f, zf = dist + 2.0f;
    float sx = 1.0f / size, sy = 1.0f / size;
    float sz = 2.0f / (zf - zn);
    float tz = -(zf + zn) / (zf - zn);

    // Combined view-projection, column-major
    mLightViewProj = {
        rX * sx,        uX * sy,        fwdX * sz,     0,
        rY * sx,        uY * sy,        fwdY * sz,     0,
        rZ * sx,        uZ * sy,        fwdZ * sz,     0,
        -(rX*lpx + rY*lpy + rZ*lpz) * sx,
        -(uX*lpx + uY*lpy + uZ*lpz) * sy,
        -(fwdX*lpx + fwdY*lpy + fwdZ*lpz) * sz + tz,
        1.0f
    };
}

void Pipeline::renderShadowPass() {
    auto* shadowProg = ShaderManager::instance().shadow();
    if (!shadowProg) return;

    mShadowMap.bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LESS);

    for (auto* item : mItems) {
        if (!item->visible || !item->castShadow()) continue;
        item->onShadowPass(mLightViewProj, kIdentity);
    }
}

void Pipeline::execute(const std::array<float, 16>& proj,
                       const std::array<float, 16>& view) {
    renderShadowPass();
    RenderTarget::bindScreen(mViewportW, mViewportH);

    // ── Frame setup ──
    glFrontFace(GL_CW);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ── Main pass (ground + model) ──
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, mShadowMap.depthTex());

    for (auto* item : mItems) {
        if (!item->visible) continue;
        item->onMainPass(proj, view, kIdentity, mLightViewProj, true);
    }

    // ── Debug pass (axis + physics) ──
    for (auto* item : mItems) {
        if (!item->visible) continue;
        item->onDebugPass(proj, view, kIdentity);
    }
}
