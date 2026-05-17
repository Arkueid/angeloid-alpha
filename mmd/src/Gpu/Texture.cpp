#include "Gpu/Texture.h"

namespace Gpu {

Texture::Texture(int w, int h, int comps, const void* data, GLenum dtype)
    : width(w), height(h), components(comps)
{
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    bool isFloat = (dtype == GL_FLOAT);
    GLenum ifmt = internalFromComps(comps, isFloat);
    GLenum fmt = formatFromComps(comps, isFloat);

    glTexImage2D(GL_TEXTURE_2D, 0, ifmt, width, height, 0, fmt, dtype, data);
}

Texture::~Texture() { destroy(); }

Texture::Texture(Texture&& other) noexcept
    : id(other.id), width(other.width), height(other.height), components(other.components)
{
    other.id = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        destroy();
        id = other.id; other.id = 0;
        width = other.width; height = other.height;
        components = other.components;
    }
    return *this;
}

void Texture::bind(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
}

void Texture::setFilter(GLenum minFilter, GLenum magFilter) const
{
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
}

void Texture::setWrap(bool repeatX, bool repeatY) const
{
    glBindTexture(GL_TEXTURE_2D, id);
    GLenum wrapS = repeatX ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    GLenum wrapT = repeatY ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
}

void Texture::setMirrorWrap(bool mirrorX, bool mirrorY) const
{
    glBindTexture(GL_TEXTURE_2D, id);
    GLenum wrapS = mirrorX ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE;
    GLenum wrapT = mirrorY ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
}

void Texture::write(const void* data, int x, int y) const
{
    glBindTexture(GL_TEXTURE_2D, id);
    GLenum fmt = formatFromComps(components);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, fmt, GL_FLOAT, data);
}

void Texture::destroy()
{
    if (id) {
        glDeleteTextures(1, &id);
        id = 0;
    }
}

GLenum Texture::formatFromComps(int comps, bool isFloat)
{
    (void)isFloat;
    switch (comps) {
        case 1: return GL_RED;
        case 2: return GL_RG;
        case 3: return GL_RGB;
        case 4: default: return GL_RGBA;
    }
}

GLenum Texture::internalFromComps(int comps, bool isFloat)
{
    if (isFloat) {
        switch (comps) {
            case 1: return GL_R32F;
            case 2: return GL_RG32F;
            case 3: return GL_RGB32F;
            case 4: default: return GL_RGBA32F;
        }
    }
    switch (comps) {
        case 1: return GL_R8;
        case 2: return GL_RG8;
        case 3: return GL_RGB8;
        case 4: default: return GL_RGBA8;
    }
}

} // namespace Gpu
