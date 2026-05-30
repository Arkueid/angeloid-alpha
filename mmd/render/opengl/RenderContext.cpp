#include "render/opengl/RenderContext.h"

#include "util/Log.h"

#include <stb_image.h>

#include <cstdio>

namespace fs = std::filesystem;

namespace mmd {

RenderContext& RenderContext::instance() {
    static RenderContext ctx;
    return ctx;
}

void RenderContext::init(const fs::path& shaderDir, const fs::path& toonDir) {
    mShaderManager = std::make_unique<ShaderManager>(shaderDir);
    MMD_INFO("RENDER", "Shaders initialized");

    // Load shared toon textures (toon01.bmp ~ toon10.bmp)
    if (!toonDir.empty()) {
        for (int ti = 1; ti <= 10; ++ti) {
            char buf[32];
            snprintf(buf, sizeof(buf), "toon%02d.bmp", ti);
            fs::path toonPath = toonDir / buf;
            int w, h, comp;
            uint8_t* data = stbi_load(toonPath.string().c_str(), &w, &h, &comp, 4);
            if (data) {
                auto tex = std::make_unique<Gpu::Texture>(w, h, 4, data);
                tex->setFilter(GL_LINEAR, GL_LINEAR);
                tex->setWrap(true, true);
                mSharedToons[ti - 1] = std::move(tex);
                stbi_image_free(data);
            }
        }
        MMD_INFO("RENDER", "Shared toon textures loaded");
    }
}

void RenderContext::release() {
    for (auto& t : mSharedToons)
        t.reset();
    mShaderManager.reset();
    MMD_INFO("RENDER", "GPU resources released");
}

Gpu::ShaderProgram* RenderContext::shader(const std::string& name) {
    return mShaderManager ? mShaderManager->get(name) : nullptr;
}

Gpu::Texture* RenderContext::gradientTexture() {
    return mShaderManager ? mShaderManager->gradientTexture() : nullptr;
}

Gpu::Texture* RenderContext::sharedToon(int index) {
    if (index < 0 || index >= 10)
        return nullptr;
    return mSharedToons[index].get();
}

}  // namespace mmd
