#include "framework/gpu/opengl/GlRenderTarget.h"
#include "framework/gpu/opengl/GlTexture.h"
#include "framework/gpu/opengl/GlTypes.h"

namespace Gpu {

GlRenderTarget::~GlRenderTarget() {
    destroy();
}

void GlRenderTarget::resize(int w, int h, bool withColor, bool withDepth) {
    if (w == mW && h == mH && withColor == mHasColor && withDepth == mHasDepth)
        return;

    destroy();

    mW = w; mH = h;
    mHasColor = withColor;
    mHasDepth = withDepth;

    glGenFramebuffers(1, &mFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);

    if (withColor) {
        mColorTex = std::make_unique<GlTexture>(w, h, TextureFormat::RGBA8, nullptr);
        glBindTexture(GL_TEXTURE_2D, mColorTex->id());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mColorTex->id(), 0);
    }

    if (withDepth) {
        mDepthTex = std::make_unique<GlTexture>(w, h, TextureFormat::Depth24, nullptr);
        glBindTexture(GL_TEXTURE_2D, mDepthTex->id());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // LINEAR enables hardware 2x2 PCF via sampler2DShadow
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               mDepthTex->id(), 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GlRenderTarget::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    glViewport(0, 0, mW, mH);
}

IGpuTexture* GlRenderTarget::colorTexture() {
    return mColorTex.get();
}

IGpuTexture* GlRenderTarget::depthTexture() {
    return mDepthTex.get();
}

void GlRenderTarget::bindScreen(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
}

void GlRenderTarget::destroy() {
    mColorTex.reset();
    mDepthTex.reset();
    if (mFbo) {
        glDeleteFramebuffers(1, &mFbo);
        mFbo = 0;
    }
}

}  // namespace Gpu
