#pragma once

namespace Gpu {

class IGpuTexture;

class IGpuRenderTarget {
public:
    virtual ~IGpuRenderTarget() = default;

    // Bind this render target for drawing.
    virtual void bind() = 0;

    // May return nullptr if this RT doesn't have a color/depth attachment.
    virtual IGpuTexture* colorTexture() = 0;
    virtual IGpuTexture* depthTexture() = 0;
};

}  // namespace Gpu