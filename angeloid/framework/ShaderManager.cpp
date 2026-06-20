#include "framework/ShaderManager.h"

#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/ShaderUtil.h"        // readShaderFile
#include "framework/gpu/Types.h"
#include "framework/util/CfgParser.h"
#include "core/util/Log.h"

#include <chrono>
#include <cstdio>
#include <stb_image.h>

namespace fs = std::filesystem;

ShaderManager& ShaderManager::instance() {
    static ShaderManager mgr;
    return mgr;
}

Gpu::IGpuShader* ShaderManager::compile(const fs::path& shaderDir,
                                         const std::string& vert,
                                         const std::string& frag) {
    auto vs = Gpu::readShaderFile(shaderDir / vert);
    auto fs = Gpu::readShaderFile(shaderDir / frag);
    if (vs.empty() || fs.empty()) {
        MMD_WARN("SHADER", "Failed to read %s / %s", vert.c_str(), frag.c_str());
        return nullptr;
    }
    auto prog = Gpu::device()->createShader(vs, fs);
    auto* ptr = prog.get();
    mPrograms.push_back(std::move(prog));
    return ptr;
}

void ShaderManager::init(const fs::path& effectsCfg, const fs::path& shaderDir,
                          const fs::path& toonDir) {
    auto t0 = std::chrono::steady_clock::now();
    auto sections = parseCfgSections(effectsCfg);

    auto get = [&](const char* name) -> std::unordered_map<std::string, std::string>* {
        auto it = sections.find(name);
        return it != sections.end() ? &it->second : nullptr;
    };

    if (auto* c = get("shadow"))
        mShadow = compile(shaderDir, (*c)["vert"], (*c)["frag"]);
    if (auto* c = get("outline"))
        mOutline = compile(shaderDir, (*c)["vert"], (*c)["frag"]);
    if (auto* c = get("base"))
        mMain = compile(shaderDir, (*c)["vert"], (*c)["frag"]);
    if (auto* c = get("toon"))
        mMainToon = compile(shaderDir, (*c)["vert"], (*c)["frag"]);
    if (auto* c = get("rigidbody"))
        mRigidBody = compile(shaderDir, (*c)["vert"], (*c)["frag"]);
    if (auto* c = get("ground"))
        mGround = compile(shaderDir, (*c)["vert"], (*c)["frag"]);
    if (auto* c = get("axis"))
        mAxis = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    auto elapsed = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    MMD_INFO("SHADER", "Loaded %zu programs in %.1f ms", mPrograms.size(), elapsed);

    // Shared textures
    createGradientTexture();

    if (!toonDir.empty()) {
        for (int ti = 0; ti <= 10; ++ti) {
            char buf[32];
            snprintf(buf, sizeof(buf), "toon%02d.bmp", ti);
            fs::path toonPath = toonDir / buf;
            int w, h, comp;
            uint8_t* data = stbi_load(toonPath.string().c_str(), &w, &h, &comp, 4);
            if (data) {
                auto tex = Gpu::device()->createTexture(w, h, Gpu::TextureFormat::RGBA8, data);
                tex->setFilter(Gpu::TextureFilter::Linear, Gpu::TextureFilter::Linear);
                tex->setWrap(Gpu::TextureWrap::Repeat, Gpu::TextureWrap::Repeat);
                mSharedToons[ti] = std::move(tex);
                stbi_image_free(data);
            }
        }
        MMD_INFO("SHADER", "Shared toon textures loaded");
    }
}

void ShaderManager::clear() {
    int toonCount = 0;
    for (auto& t : mSharedToons) {
        if (t) ++toonCount;
        t.reset();
    }
    mGradient.reset();

    MMD_INFO("SHADER", "Releasing %zu programs, gradient, %d toon textures",
             mPrograms.size(), toonCount);
    mPrograms.clear();
    mShadow = nullptr;
    mOutline = nullptr;
    mMain = nullptr;
    mMainToon = nullptr;
    mRigidBody = nullptr;
    mGround = nullptr;
    mAxis = nullptr;
}

void ShaderManager::createGradientTexture() {
    // 4-level gray gradient for cel-shading ramp in toon fragment shader.
    uint8_t gradient[] = {
        60, 60, 60, 255, 120, 120, 120, 255,
        180, 180, 180, 255, 220, 220, 220, 255,
    };
    mGradient = Gpu::device()->createTexture(4, 1, Gpu::TextureFormat::RGBA8, gradient);
    mGradient->setFilter(Gpu::TextureFilter::Linear, Gpu::TextureFilter::Linear);
    mGradient->setWrap(Gpu::TextureWrap::Clamp, Gpu::TextureWrap::Clamp);
}

Gpu::IGpuTexture* ShaderManager::sharedToon(int index) {
    if (index < 0 || index > 10)
        return nullptr;
    return mSharedToons[index].get();
}
