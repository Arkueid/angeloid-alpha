#include "framework/opengl/RenderContext.h"

#include "core/util/Log.h"

#include <GL/glew.h>
#include <stb_image.h>

#include <cstdio>

namespace fs = std::filesystem;

namespace mmd {

RenderContext& RenderContext::instance() {
    static RenderContext ctx;
    return ctx;
}

void RenderContext::init(const fs::path& toonDir) {
    createGradientTexture();

    // Load shared toon textures (toon01.bmp ~ toon10.bmp)
    if (!toonDir.empty()) {
        for (int ti = 0; ti <= 10; ++ti) {
            char buf[32];
            snprintf(buf, sizeof(buf), "toon%02d.bmp", ti);
            fs::path toonPath = toonDir / buf;
            int w, h, comp;
            uint8_t* data = stbi_load(toonPath.string().c_str(), &w, &h, &comp, 4);
            if (data) {
                auto tex = std::make_unique<Gpu::Texture>(w, h, 4, data);
                tex->setFilter(GL_LINEAR, GL_LINEAR);
                tex->setWrap(true, true);
                mSharedToons[ti] = std::move(tex);
                stbi_image_free(data);
            }
        }
        MMD_INFO("RENDER", "Shared toon textures loaded");
    }
}

void RenderContext::release() {
    for (auto& t : mSharedToons)
        t.reset();
    mGradient.reset();
    MMD_INFO("RENDER", "GPU resources released");
}

void RenderContext::createGradientTexture() {
    // 4-level gray gradient for cel-shading ramp in toon fragment shader.
    uint8_t gradient[] = {
        60, 60, 60, 120, 120, 120, 180, 180, 180, 220, 220, 220,
    };
    mGradient = std::make_unique<Gpu::Texture>(4, 1, 3, gradient);
    mGradient->setFilter(GL_LINEAR, GL_LINEAR);
    mGradient->setWrap(false, false);
}

Gpu::Texture* RenderContext::gradientTexture() {
    return mGradient.get();
}

Gpu::Texture* RenderContext::sharedToon(int index) {
    if (index < 0 || index > 10)
        return nullptr;
    return mSharedToons[index].get();
}

}  // namespace mmd
