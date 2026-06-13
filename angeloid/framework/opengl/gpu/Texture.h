#pragma once

#include <GL/glew.h>
#include <cstdint>

namespace Gpu {

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int components = 0;

    Texture(int w, int h, int comps, const void* data = nullptr, GLenum dtype = GL_UNSIGNED_BYTE);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&&) noexcept;
    Texture& operator=(Texture&&) noexcept;

    void bind(int unit = 0) const;
    void setFilter(GLenum minFilter, GLenum magFilter) const;
    void setWrap(bool repeatX, bool repeatY) const;
    void setMirrorWrap(bool mirrorX, bool mirrorY) const;
    void write(const void* data, int x = 0, int y = 0) const;
    void destroy();

    static GLenum formatFromComps(int comps, bool isFloat = false);
    static GLenum internalFromComps(int comps, bool isFloat = false);
};

}  // namespace Gpu
