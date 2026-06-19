#include "framework/gpu/opengl/GlTexture.h"
#include "framework/gpu/opengl/GlTypes.h"

namespace Gpu {

GlTexture::GlTexture(int w, int h, TextureFormat fmt, const void* data)
    : mWidth(w), mHeight(h), mFormat(fmt) {
    glGenTextures(1, &mId);
    glBindTexture(GL_TEXTURE_2D, mId);

    GLenum ifmt = toGlTextureInternalFormat(fmt);
    GLenum gpuFmt = toGlTextureFormat(fmt);
    GLenum dtype = toGlTextureDtype(fmt);

    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, gpuFmt, dtype, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}

GlTexture::~GlTexture() {
    destroy();
}

GlTexture::GlTexture(GlTexture&& other) noexcept
    : mId(other.mId), mWidth(other.mWidth), mHeight(other.mHeight), mFormat(other.mFormat) {
    other.mId = 0;
}

GlTexture& GlTexture::operator=(GlTexture&& other) noexcept {
    if (this != &other) {
        destroy();
        mId = other.mId; other.mId = 0;
        mWidth = other.mWidth;
        mHeight = other.mHeight;
        mFormat = other.mFormat;
    }
    return *this;
}

void GlTexture::bind(int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, mId);
}

void GlTexture::setFilter(TextureFilter minFilter, TextureFilter magFilter) {
    glBindTexture(GL_TEXTURE_2D, mId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, toGlFilter(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, toGlFilter(magFilter));
}

void GlTexture::setWrap(TextureWrap s, TextureWrap t) {
    glBindTexture(GL_TEXTURE_2D, mId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, toGlWrap(s));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, toGlWrap(t));
}

void GlTexture::setMirrorWrap(bool mirrorX, bool mirrorY) {
    glBindTexture(GL_TEXTURE_2D, mId);
    GLenum ws = mirrorX ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE;
    GLenum wt = mirrorY ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ws);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wt);
}

void GlTexture::write(const void* data) {
    glBindTexture(GL_TEXTURE_2D, mId);
    GLenum gpuFmt = toGlTextureFormat(mFormat);
    GLenum dtype = toGlTextureDtype(mFormat);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, gpuFmt, dtype, data);
}

void GlTexture::destroy() {
    if (mId) {
        glDeleteTextures(1, &mId);
        mId = 0;
    }
}

}  // namespace Gpu
