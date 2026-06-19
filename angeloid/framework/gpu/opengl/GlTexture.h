#pragma once

#include "framework/gpu/IGpuTexture.h"

#include <glad/glad.h>

namespace Gpu {

class GlTexture : public IGpuTexture {
public:
    GlTexture(int w, int h, TextureFormat fmt, const void* data);
    ~GlTexture() override;

    GlTexture(const GlTexture&) = delete;
    GlTexture& operator=(const GlTexture&) = delete;
    GlTexture(GlTexture&&) noexcept;
    GlTexture& operator=(GlTexture&&) noexcept;

    GLuint id() const { return mId; }

    int width() const override  { return mWidth; }
    int height() const override { return mHeight; }

    void bind(int unit) override;
    void setFilter(TextureFilter minFilter, TextureFilter magFilter) override;
    void setWrap(TextureWrap s, TextureWrap t) override;
    void setMirrorWrap(bool mirrorX, bool mirrorY) override;
    void write(const void* data) override;

    void destroy();

private:
    GLuint mId = 0;
    int mWidth = 0;
    int mHeight = 0;
    TextureFormat mFormat = TextureFormat::RGBA8;
};

}  // namespace Gpu
