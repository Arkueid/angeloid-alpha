#include "framework/Pipeline.h"

#include "framework/Camera.h"
#include <cmath>

#include "core/math/VecMath.h"
#include "framework/Renderable.h"
#include "framework/ShaderManager.h"
#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/Types.h"

static constexpr int kShadowMapSize = 4096;
static const std::array<float, 16> kIdentity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

Pipeline& Pipeline::instance() {
    static Pipeline p;
    return p;
}

void Pipeline::init() {
    auto* dev = Gpu::device();
    mShadowMap = dev->createRenderTarget(kShadowMapSize, kShadowMapSize, false, true);
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

bool Pipeline::collectShadowBounds(Vec3& outMin, Vec3& outMax) const {
    outMin = {1e9f, 1e9f, 1e9f};
    outMax = {-1e9f, -1e9f, -1e9f};
    bool has = false;
    for (size_t i = 0; i < mItems.size(); ++i) {
        auto* item = mItems[i];
        Vec3 mn, mx;
        if (item->castShadow() && item->shadowBounds(mn, mx)) {
            outMin.x = std::min(outMin.x, mn.x);
            outMin.y = std::min(outMin.y, mn.y);
            outMin.z = std::min(outMin.z, mn.z);
            outMax.x = std::max(outMax.x, mx.x);
            outMax.y = std::max(outMax.y, mx.y);
            outMax.z = std::max(outMax.z, mx.z);
            has = true;
        }
    }
    return has;
}

void Pipeline::render(int screenW, int screenH) {
    resizeViewport(screenW, screenH);
    Vec3 smin, smax;
    bool has = collectShadowBounds(smin, smax);
    computeLightMatrix(lightDir, has ? &smin.x : nullptr, has ? &smax.x : nullptr);
    auto proj = Camera::projectionMatrix(screenW, screenH);
    auto view = Camera::instance().viewMatrix();
    execute(proj, view);
}

void Pipeline::resizeViewport(int w, int h) {
    mViewportW = w;
    mViewportH = h;
}

void Pipeline::computeLightMatrix(const float* ldir,
                                   const float* sceneMin,
                                   const float* sceneMax) {
    float lx = ldir[0], ly = ldir[1], lz = ldir[2];
    float len = std::sqrt(lx*lx + ly*ly + lz*lz);
    if (len < 1e-8f) { lx = 0; ly = 1; lz = 0; }
    else { lx /= len; ly /= len; lz /= len; }

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
    if (sceneMin && sceneMax && sceneMin[0] != sceneMax[0]) {
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

    auto* dev = Gpu::device();
    mShadowMap->bind();
    dev->clear(false, true);
    dev->setDepthFunc(Gpu::CompareFunc::Less);

    ShadowPassParams sp{mLightViewProj, kIdentity};
    for (auto* item : mItems) {
        if (!item->visible || !item->castShadow()) continue;
        item->onShadowPass(sp);
    }
}

void Pipeline::execute(const std::array<float, 16>& proj,
                       const std::array<float, 16>& view) {
    auto* dev = Gpu::device();
    bool anyShadow = showSelfShadow || showGroundShadow;
    if (anyShadow)
        renderShadowPass();
    dev->bindScreenFramebuffer(mViewportW, mViewportH);

    // ── Frame setup ──
    dev->setFrontFace(true);  // CW front face (MMD convention)
    dev->setDepthTest(true);
    dev->setDepthFunc(Gpu::CompareFunc::LEqual);
    dev->setBlend(true);
    dev->setBlendFunc(Gpu::BlendFactor::SrcAlpha, Gpu::BlendFactor::OneMinusSrcAlpha);
    dev->setClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    dev->clear(true, true);

    // ── Main pass (ground + model) ──
    if (anyShadow) {
        dev->bindTextureToUnit(5, mShadowMap->depthTexture());
    }

    std::array<float, 3> ldir = {lightDir[0], lightDir[1], lightDir[2]};
    MainPassParams mp{proj, view, kIdentity, mLightViewProj, ldir, false};
    for (auto* item : mItems) {
        if (!item->visible) continue;
        mp.hasShadow = item->castShadow() ? showSelfShadow : showGroundShadow;
        item->onMainPass(mp);
    }

    // ── Debug pass (axis + physics) ──
    DebugPassParams dp{proj, view, kIdentity};
    for (auto* item : mItems) {
        if (!item->visible) continue;
        item->onDebugPass(dp);
    }
}
