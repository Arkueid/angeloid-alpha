#pragma once

#include "render/opengl/ShaderManager.h"
#include "render/opengl/gpu/Texture.h"

#include <filesystem>
#include <memory>

namespace mmd {

// Singleton holding GPU resources: shader programs, shared toon textures, gradient.
// Must call init() while GL context is alive; release() before GL context destroyed.
class RenderContext {
public:
    static RenderContext& instance();

    void init(const std::filesystem::path& shaderDir,
              const std::filesystem::path& toonDir);
    void release();

    Gpu::ShaderProgram* shader(const std::string& name);
    Gpu::Texture* gradientTexture();
    Gpu::Texture* sharedToon(int index);  // 0-9, may return nullptr

private:
    RenderContext() = default;
    ~RenderContext() = default;
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    std::unique_ptr<ShaderManager> mShaderManager;
    std::unique_ptr<Gpu::Texture> mSharedToons[10];
};

}  // namespace mmd
