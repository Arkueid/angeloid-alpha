#include "framework/ShaderManager.h"

#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/ShaderUtil.h"        // readShaderFile
#include "framework/util/CfgParser.h"
#include "core/util/Log.h"

#include <chrono>

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

void ShaderManager::init(const fs::path& effectsCfg, const fs::path& shaderDir) {
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
}

void ShaderManager::clear() {
    mPrograms.clear();
    mShadow = nullptr;
    mOutline = nullptr;
    mMain = nullptr;
    mMainToon = nullptr;
    mRigidBody = nullptr;
    mGround = nullptr;
    mAxis = nullptr;
}
