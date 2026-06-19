#pragma once

#include "framework/gpu/IGpuShader.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// ──── ShaderManager — global shader program registry ────
//
//   Owns all GPU shader programs loaded from effects.cfg.
//   Renderables query the shader they need by name; Pipeline drives
//   the pass ordering and calls each Renderable's pass callback.

class ShaderManager {
public:
    static ShaderManager& instance();

    void init(const std::filesystem::path& effectsCfg,
              const std::filesystem::path& shaderDir);
    void clear();

    // Convenience accessors
    Gpu::IGpuShader* shadow()   const { return mShadow; }
    Gpu::IGpuShader* outline()  const { return mOutline; }
    Gpu::IGpuShader* main()     const { return mMain; }
    Gpu::IGpuShader* toon()     const { return mMainToon; }
    Gpu::IGpuShader* rigidBody()  const { return mRigidBody; }
    Gpu::IGpuShader* ground()   const { return mGround; }
    Gpu::IGpuShader* axis()     const { return mAxis; }

private:
    ShaderManager() = default;

    Gpu::IGpuShader* compile(const std::filesystem::path& shaderDir,
                              const std::string& vert,
                              const std::string& frag);

    Gpu::IGpuShader* mShadow = nullptr;
    Gpu::IGpuShader* mOutline = nullptr;
    Gpu::IGpuShader* mMain = nullptr;
    Gpu::IGpuShader* mMainToon = nullptr;
    Gpu::IGpuShader* mRigidBody = nullptr;
    Gpu::IGpuShader* mGround = nullptr;
    Gpu::IGpuShader* mAxis = nullptr;

    std::vector<std::unique_ptr<Gpu::IGpuShader>> mPrograms;
};
