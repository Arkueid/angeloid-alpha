#pragma once

#include "framework/opengl/gpu/Shader.h"

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
    Gpu::ShaderProgram* shadow()   const { return mShadow; }
    Gpu::ShaderProgram* outline()  const { return mOutline; }
    Gpu::ShaderProgram* main()     const { return mMain; }
    Gpu::ShaderProgram* toon()     const { return mMainToon; }
    Gpu::ShaderProgram* rigidBody()  const { return mRigidBody; }
    Gpu::ShaderProgram* ground()   const { return mGround; }
    Gpu::ShaderProgram* axis()     const { return mAxis; }

private:
    ShaderManager() = default;

    Gpu::ShaderProgram* compile(const std::filesystem::path& shaderDir,
                                const std::string& vert,
                                const std::string& frag);

    Gpu::ShaderProgram* mShadow = nullptr;
    Gpu::ShaderProgram* mOutline = nullptr;
    Gpu::ShaderProgram* mMain = nullptr;
    Gpu::ShaderProgram* mMainToon = nullptr;
    Gpu::ShaderProgram* mRigidBody = nullptr;
    Gpu::ShaderProgram* mGround = nullptr;
    Gpu::ShaderProgram* mAxis = nullptr;

    std::vector<std::unique_ptr<Gpu::ShaderProgram>> mPrograms;
};
