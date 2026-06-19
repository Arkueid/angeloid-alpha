#pragma once

#include "framework/gpu/Types.h"

namespace Gpu {

class IGpuTexture {
public:
    virtual ~IGpuTexture() = default;

    virtual int width() const = 0;
    virtual int height() const = 0;

    // Bind this texture to the given texture unit slot.
    virtual void bind(int unit) = 0;

    virtual void setFilter(TextureFilter minFilter, TextureFilter magFilter) = 0;
    virtual void setWrap(TextureWrap s, TextureWrap t) = 0;
    virtual void setMirrorWrap(bool mirrorX, bool mirrorY) = 0;

    // Write pixel data to the entire texture (re-allocates at current size).
    virtual void write(const void* data) = 0;
};

}  // namespace Gpu