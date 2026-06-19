#pragma once

#include "framework/gpu/IGpuTexture.h"

#include <filesystem>
#include <memory>

namespace mmd {

// Singleton holding GPU resources: gradient texture, shared toon textures.
// Must call init() while GL context is alive; release() before GL context destroyed.
class RenderContext {
public:
    static RenderContext& instance();

    void init(const std::filesystem::path& toonDir);
    void release();

    Gpu::IGpuTexture* gradientTexture();
    Gpu::IGpuTexture* sharedToon(int index);  // 0-10, may return nullptr

private:
    RenderContext() = default;
    ~RenderContext() = default;
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    void createGradientTexture();

    std::unique_ptr<Gpu::IGpuTexture> mGradient;
    std::unique_ptr<Gpu::IGpuTexture> mSharedToons[11];
};

}  // namespace mmd
