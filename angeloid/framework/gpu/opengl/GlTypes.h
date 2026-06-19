#pragma once

#include "framework/gpu/Types.h"

#include <glad/glad.h>

namespace Gpu {

// Translate Gpu enums to OpenGL GLenum values.
// These helpers isolate GL-specific constants in one place.

inline GLenum toGlBufferUsage(BufferUsage u) {
    return u == BufferUsage::Dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
}

inline GLenum toGlIndexType(IndexType t) {
    return t == IndexType::UInt16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

inline int toGlIndexBytes(IndexType t) {
    return t == IndexType::UInt16 ? 2 : 4;
}

inline GLenum toGlPrimitive(PrimitiveType p) {
    return p == PrimitiveType::Lines ? GL_LINES : GL_TRIANGLES;
}

inline GLenum toGlDataType(DataType d) {
    switch (d) {
    case DataType::Float:  return GL_FLOAT;
    case DataType::Int8:   return GL_BYTE;
    case DataType::UInt8:  return GL_UNSIGNED_BYTE;
    case DataType::Int16:  return GL_SHORT;
    case DataType::UInt16: return GL_UNSIGNED_SHORT;
    case DataType::Int32:  return GL_INT;
    case DataType::UInt32: return GL_UNSIGNED_INT;
    }
    return GL_FLOAT;
}

inline bool isIntegerDataType(DataType d) {
    switch (d) {
    case DataType::Float:  return false;
    default:               return true;
    }
}

inline GLenum toGlTextureInternalFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::R8:      return GL_R8;
    case TextureFormat::RG8:     return GL_RG8;
    case TextureFormat::RGB8:    return GL_RGB8;
    case TextureFormat::RGBA8:   return GL_RGBA8;
    case TextureFormat::R32F:    return GL_R32F;
    case TextureFormat::RG32F:   return GL_RG32F;
    case TextureFormat::RGB32F:  return GL_RGB32F;
    case TextureFormat::RGBA32F: return GL_RGBA32F;
    case TextureFormat::Depth24: return GL_DEPTH_COMPONENT24;
    }
    return GL_RGBA8;
}

inline GLenum toGlTextureFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::R8:      return GL_RED;
    case TextureFormat::RG8:     return GL_RG;
    case TextureFormat::RGB8:    return GL_RGB;
    case TextureFormat::RGBA8:   return GL_RGBA;
    case TextureFormat::R32F:    return GL_RED;
    case TextureFormat::RG32F:   return GL_RG;
    case TextureFormat::RGB32F:  return GL_RGB;
    case TextureFormat::RGBA32F: return GL_RGBA;
    case TextureFormat::Depth24: return GL_DEPTH_COMPONENT;
    }
    return GL_RGBA;
}

inline GLenum toGlTextureDtype(TextureFormat f) {
    switch (f) {
    case TextureFormat::R32F:
    case TextureFormat::RG32F:
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
        return GL_FLOAT;
    case TextureFormat::Depth24:
        return GL_UNSIGNED_INT;
    default:
        return GL_UNSIGNED_BYTE;
    }
}

inline GLenum toGlFilter(TextureFilter f) {
    switch (f) {
    case TextureFilter::Nearest:           return GL_NEAREST;
    case TextureFilter::Linear:            return GL_LINEAR;
    case TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

inline GLenum toGlWrap(TextureWrap w) {
    switch (w) {
    case TextureWrap::Clamp:        return GL_CLAMP_TO_EDGE;
    case TextureWrap::Repeat:       return GL_REPEAT;
    case TextureWrap::MirrorRepeat: return GL_MIRRORED_REPEAT;
    }
    return GL_CLAMP_TO_EDGE;
}

inline GLenum toGlCompareFunc(CompareFunc f) {
    switch (f) {
    case CompareFunc::Never:    return GL_NEVER;
    case CompareFunc::Less:     return GL_LESS;
    case CompareFunc::LEqual:   return GL_LEQUAL;
    case CompareFunc::Greater:  return GL_GREATER;
    case CompareFunc::GEqual:   return GL_GEQUAL;
    case CompareFunc::Equal:    return GL_EQUAL;
    case CompareFunc::NotEqual: return GL_NOTEQUAL;
    case CompareFunc::Always:   return GL_ALWAYS;
    }
    return GL_LEQUAL;
}

inline GLenum toGlBlendFactor(BlendFactor f) {
    switch (f) {
    case BlendFactor::Zero:             return GL_ZERO;
    case BlendFactor::One:              return GL_ONE;
    case BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::SrcColor:         return GL_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DstAlpha:         return GL_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
    case BlendFactor::DstColor:         return GL_DST_COLOR;
    case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
    }
    return GL_ONE;
}

inline GLenum toGlCullMode(CullMode m) {
    switch (m) {
    case CullMode::None:  return GL_NONE;  // sentinel; caller disables GL_CULL_FACE
    case CullMode::Front: return GL_FRONT;
    case CullMode::Back:  return GL_BACK;
    }
    return GL_BACK;
}

inline GLenum toGlPolygonMode(PolygonMode m) {
    switch (m) {
    case PolygonMode::Fill: return GL_FILL;
    case PolygonMode::Line: return GL_LINE;
    }
    return GL_FILL;
}

}  // namespace Gpu
