#pragma once

#include "framework/opengl/gpu/Texture.h"

#include <GL/glew.h>
#include <memory>

// Wraps an FBO with color + depth attachments.
// Resize (or first create) destroys old FBO and creates new with given dimensions.

class RenderTarget {
public:
    RenderTarget() = default;
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // Create or recreate FBO at new size. Idempotent if size unchanged.
    void resize(int w, int h, bool withColor = true, bool withDepth = true);

    void bind();
    static void bindScreen(int w, int h);

    GLuint colorTex() const { return mColorTex; }
    GLuint depthTex() const { return mDepthTex; }
    int width()  const { return mW; }
    int height() const { return mH; }

private:
    GLuint mFbo = 0;
    GLuint mColorTex = 0;
    GLuint mDepthTex = 0;
    int mW = 0, mH = 0;
    bool mHasColor = false, mHasDepth = false;
};
