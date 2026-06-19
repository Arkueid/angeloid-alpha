#pragma once

#include "framework/gpu/IGpuRenderTarget.h"

#include <glad/glad.h>
#include <memory>

namespace Gpu {

class GlTexture;

class GlRenderTarget : public IGpuRenderTarget {
public:
    GlRenderTarget() = default;
    ~GlRenderTarget() override;

    GlRenderTarget(const GlRenderTarget&) = delete;
    GlRenderTarget& operator=(const GlRenderTarget&) = delete;

    void resize(int w, int h, bool withColor, bool withDepth);

    void bind() override;
    IGpuTexture* colorTexture() override;
    IGpuTexture* depthTexture() override;

    GLuint fbo() const { return mFbo; }
    int width() const  { return mW; }
    int height() const { return mH; }

    static void bindScreen(int w, int h);

private:
    void destroy();

    GLuint mFbo = 0;
    std::unique_ptr<GlTexture> mColorTex;
    std::unique_ptr<GlTexture> mDepthTex;
    int mW = 0, mH = 0;
    bool mHasColor = false, mHasDepth = false;
};

}  // namespace Gpu
