#include "framework/opengl/RenderTarget.h"

RenderTarget::~RenderTarget() {
    if (mDepthTex) glDeleteTextures(1, &mDepthTex);
    if (mColorTex) glDeleteTextures(1, &mColorTex);
    if (mFbo)      glDeleteFramebuffers(1, &mFbo);
}

void RenderTarget::resize(int w, int h, bool withColor, bool withDepth) {
    if (w == mW && h == mH && withColor == mHasColor && withDepth == mHasDepth)
        return;

    // Destroy old
    if (mDepthTex) { glDeleteTextures(1, &mDepthTex); mDepthTex = 0; }
    if (mColorTex) { glDeleteTextures(1, &mColorTex); mColorTex = 0; }
    if (mFbo)      { glDeleteFramebuffers(1, &mFbo);      mFbo = 0; }

    mW = w; mH = h;
    mHasColor = withColor;
    mHasDepth = withDepth;

    glGenFramebuffers(1, &mFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);

    if (withColor) {
        glGenTextures(1, &mColorTex);
        glBindTexture(GL_TEXTURE_2D, mColorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTex, 0);
    }

    if (withDepth) {
        glGenTextures(1, &mDepthTex);
        glBindTexture(GL_TEXTURE_2D, mDepthTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mDepthTex, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::bind() {
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    glViewport(0, 0, mW, mH);
}

void RenderTarget::bindScreen(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
}
