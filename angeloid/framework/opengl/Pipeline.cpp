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

void Pipeline::computeLightMatrix(const float* lightDir,
                                   const float* sceneMin,
                                   const float* sceneMax) {
    // Normalize light direction
    float lx = lightDir[0], ly = lightDir[1], lz = lightDir[2];
    float len = std::sqrt(lx*lx + ly*ly + lz*lz);
    lx /= len; ly /= len; lz /= len;

    // Build light view basis (look-at from light position toward origin)
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

    // Compute light-space AABB from scene bounds (or use defaults)
    float lsMinX, lsMaxX, lsMinY, lsMaxY, lsMinZ, lsMaxZ;
    if (sceneMin && sceneMax) {
        lsMinX = 1e9f; lsMaxX = -1e9f;
        lsMinY = 1e9f; lsMaxY = -1e9f;
        lsMinZ = 1e9f; lsMaxZ = -1e9f;
        // Transform 8 world-space corners into light view space
        for (int i = 0; i < 8; ++i) {
            float wx = (i & 1) ? sceneMax[0] : sceneMin[0];
            float wy = (i & 2) ? sceneMax[1] : sceneMin[1];
            float wz = (i & 4) ? sceneMax[2] : sceneMin[2];
            // Light-view transform:  lx = dot(right, world-lightPos), etc.
            float dx = wx - lx * 15.0f;
            float dy = wy - ly * 15.0f;
            float dz = wz - lz * 15.0f;
            float lsx = rX*dx + rY*dy + rZ*dz;
            float lsy = uX*dx + uY*dy + uZ*dz;
            float lsz = fwdX*dx + fwdY*dy + fwdZ*dz;
            lsMinX = std::min(lsMinX, lsx); lsMaxX = std::max(lsMaxX, lsx);
            lsMinY = std::min(lsMinY, lsy); lsMaxY = std::max(lsMaxY, lsy);
            lsMinZ = std::min(lsMinZ, lsz); lsMaxZ = std::max(lsMaxZ, lsz);
        }
        // Add 10% padding
        float padX = (lsMaxX - lsMinX) * 0.05f + 0.1f;
        float padY = (lsMaxY - lsMinY) * 0.05f + 0.1f;
        float padZ = (lsMaxZ - lsMinZ) * 0.05f + 0.2f;
        lsMinX -= padX; lsMaxX += padX;
        lsMinY -= padY; lsMaxY += padY;
        lsMinZ -= padZ; lsMaxZ += padZ;
    } else {
        lsMinX = -3.0f; lsMaxX = 3.0f;
        lsMinY = -3.0f; lsMaxY = 3.0f;
        lsMinZ = 13.0f; lsMaxZ = 17.0f;
    }

    // Build orthographic projection from light-space AABB
    // Maps [lsMin, lsMax] → [-1, 1] for each axis
    float sx = 2.0f / (lsMaxX - lsMinX);
    float sy = 2.0f / (lsMaxY - lsMinY);
    float sz = 2.0f / (lsMaxZ - lsMinZ);
    float tx = -(lsMaxX + lsMinX) / (lsMaxX - lsMinX);
    float ty = -(lsMaxY + lsMinY) / (lsMaxY - lsMinY);
    float tz = -(lsMaxZ + lsMinZ) / (lsMaxZ - lsMinZ);

    float lpx = lx * 15.0f, lpy = ly * 15.0f, lpz = lz * 15.0f;
    float dotR = rX*lpx + rY*lpy + rZ*lpz;
    float dotU = uX*lpx + uY*lpy + uZ*lpz;
    float dotF = fwdX*lpx + fwdY*lpy + fwdZ*lpz;

    // Combined view-projection, column-major
    mLightViewProj = {
        rX * sx,            uX * sy,            fwdX * sz,      0,
        rY * sx,            uY * sy,            fwdY * sz,      0,
        rZ * sx,            uZ * sy,            fwdZ * sz,      0,
        -dotR * sx + tx,
        -dotU * sy + ty,
        -dotF * sz + tz,
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
