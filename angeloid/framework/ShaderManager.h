#pragma once

#include "framework/gpu/IGpuShader.h"
#include "framework/gpu/IGpuTexture.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// ──── ShaderManager — global GPU resource registry ────
//
//   Owns all shared GPU resources: shader programs (from effects.cfg),
//   gradient texture, and shared toon textures (toon00.bmp ~ toon10.bmp).
//   Renderables query resources by name; Pipeline drives pass ordering.

class ShaderManager {
public:
    static ShaderManager& instance();

    void init(const std::filesystem::path& effectsCfg,
              const std::filesystem::path& shaderDir,
              const std::filesystem::path& toonDir);
    void clear();

    // Shader accessors
    Gpu::IGpuShader* shadow()   const { return mShadow; }
    Gpu::IGpuShader* outline()  const { return mOutline; }
    Gpu::IGpuShader* main()     const { return mMain; }
    Gpu::IGpuShader* toon()     const { return mMainToon; }
    Gpu::IGpuShader* rigidBody()  const { return mRigidBody; }
    Gpu::IGpuShader* ground()   const { return mGround; }
    Gpu::IGpuShader* axis()     const { return mAxis; }

    // Shared texture accessors
    Gpu::IGpuTexture* gradientTexture() { return mGradient.get(); }
    Gpu::IGpuTexture* sharedToon(int index);  // 0-10, may return nullptr

private:
    ShaderManager() = default;

    Gpu::IGpuShader* compile(const std::filesystem::path& shaderDir,
                              const std::string& vert,
                              const std::string& frag);
    void createGradientTexture();

    // Shaders
    Gpu::IGpuShader* mShadow = nullptr;
    Gpu::IGpuShader* mOutline = nullptr;
    Gpu::IGpuShader* mMain = nullptr;
    Gpu::IGpuShader* mMainToon = nullptr;
    Gpu::IGpuShader* mRigidBody = nullptr;
    Gpu::IGpuShader* mGround = nullptr;
    Gpu::IGpuShader* mAxis = nullptr;
    std::vector<std::unique_ptr<Gpu::IGpuShader>> mPrograms;

    // Shared textures
    std::unique_ptr<Gpu::IGpuTexture> mGradient;
    std::unique_ptr<Gpu::IGpuTexture> mSharedToons[11];
};
