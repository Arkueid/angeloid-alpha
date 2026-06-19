#pragma once

#include "framework/gpu/Types.h"

#include <vulkan/vulkan.h>

namespace Gpu {

// ── VkTypes — Gpu enum → Vulkan constant mapping ──

inline VkFormat toVkFormat(TextureFormat f) {
    switch (f) {
    case TextureFormat::R8:      return VK_FORMAT_R8_UNORM;
    case TextureFormat::RG8:     return VK_FORMAT_R8G8_UNORM;
    case TextureFormat::RGB8:    return VK_FORMAT_R8G8B8A8_UNORM;  // Vulkan: RGB8 unsupported, use RGBA8
    case TextureFormat::RGBA8:   return VK_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R32F:    return VK_FORMAT_R32_SFLOAT;
    case TextureFormat::RG32F:   return VK_FORMAT_R32G32_SFLOAT;
    case TextureFormat::RGB32F:  return VK_FORMAT_R32G32B32A32_SFLOAT;  // Vulkan: RGB32 unsupported, use RGBA32
    case TextureFormat::RGBA32F: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case TextureFormat::Depth24: return VK_FORMAT_D32_SFLOAT;
    }
    return VK_FORMAT_R8G8B8A8_UNORM;
}

inline int vkFormatSize(TextureFormat f) {
    switch (f) {
    case TextureFormat::R8:      return 1;
    case TextureFormat::RG8:     return 2;
    case TextureFormat::RGB8:    return 4;  // padded to RGBA8
    case TextureFormat::RGBA8:   return 4;
    case TextureFormat::R32F:    return 4;
    case TextureFormat::RG32F:   return 8;
    case TextureFormat::RGB32F:  return 12;
    case TextureFormat::RGBA32F: return 16;
    case TextureFormat::Depth24: return 4;
    }
    return 4;
}

inline VkFormat toVkAttributeFormat(DataType d, int size) {
    switch (d) {
    case DataType::Float:
        switch (size) {
        case 1: return VK_FORMAT_R32_SFLOAT;
        case 2: return VK_FORMAT_R32G32_SFLOAT;
        case 3: return VK_FORMAT_R32G32B32_SFLOAT;
        case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        }
        break;
    case DataType::Int8:   return size == 1 ? VK_FORMAT_R8_SINT   : VK_FORMAT_R8G8_SINT;
    case DataType::UInt8:  return size == 1 ? VK_FORMAT_R8_UINT   : VK_FORMAT_R8G8_UINT;
    case DataType::Int16:  return size == 1 ? VK_FORMAT_R16_SINT  : VK_FORMAT_R16G16_SINT;
    case DataType::UInt16: return size == 1 ? VK_FORMAT_R16_UINT  : VK_FORMAT_R16G16_UINT;
    case DataType::Int32:  return size == 1 ? VK_FORMAT_R32_SINT  : size == 2 ? VK_FORMAT_R32G32_SINT  : size == 3 ? VK_FORMAT_R32G32B32_SINT  : VK_FORMAT_R32G32B32A32_SINT;
    case DataType::UInt32: return size == 1 ? VK_FORMAT_R32_UINT  : size == 2 ? VK_FORMAT_R32G32_UINT  : size == 3 ? VK_FORMAT_R32G32B32_UINT  : VK_FORMAT_R32G32B32A32_UINT;
    }
    return VK_FORMAT_R32G32B32_SFLOAT;
}

inline VkPrimitiveTopology toVkPrimitiveTopology(PrimitiveType p) {
    switch (p) {
    case PrimitiveType::Triangles: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveType::Lines:     return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

inline VkCompareOp toVkCompareOp(CompareFunc f) {
    switch (f) {
    case CompareFunc::Never:    return VK_COMPARE_OP_NEVER;
    case CompareFunc::Less:     return VK_COMPARE_OP_LESS;
    case CompareFunc::LEqual:   return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareFunc::Greater:  return VK_COMPARE_OP_GREATER;
    case CompareFunc::GEqual:   return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareFunc::Equal:    return VK_COMPARE_OP_EQUAL;
    case CompareFunc::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
    case CompareFunc::Always:   return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_LESS_OR_EQUAL;
}

inline VkBlendFactor toVkBlendFactor(BlendFactor f) {
    switch (f) {
    case BlendFactor::Zero:             return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:              return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::SrcColor:         return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFactor::DstColor:         return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    }
    return VK_BLEND_FACTOR_ONE;
}

inline VkCullModeFlags toVkCullMode(CullMode m) {
    switch (m) {
    case CullMode::None:  return VK_CULL_MODE_NONE;
    case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE;
}

inline VkPolygonMode toVkPolygonMode(PolygonMode m) {
    switch (m) {
    case PolygonMode::Fill: return VK_POLYGON_MODE_FILL;
    case PolygonMode::Line: return VK_POLYGON_MODE_LINE;
    }
    return VK_POLYGON_MODE_FILL;
}

inline VkFilter toVkFilter(TextureFilter f) {
    switch (f) {
    case TextureFilter::Nearest:           return VK_FILTER_NEAREST;
    case TextureFilter::Linear:            return VK_FILTER_LINEAR;
    case TextureFilter::LinearMipmapLinear: return VK_FILTER_LINEAR;
    }
    return VK_FILTER_LINEAR;
}

inline VkSamplerMipmapMode toVkSamplerMipmapMode(TextureFilter f) {
    switch (f) {
    case TextureFilter::LinearMipmapLinear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    default:                                return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    }
}

inline VkSamplerAddressMode toVkSamplerAddressMode(TextureWrap w) {
    switch (w) {
    case TextureWrap::Clamp:        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case TextureWrap::Repeat:       return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case TextureWrap::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

inline VkIndexType toVkIndexType(IndexType t) {
    switch (t) {
    case IndexType::UInt16: return VK_INDEX_TYPE_UINT16;
    case IndexType::UInt32: return VK_INDEX_TYPE_UINT32;
    }
    return VK_INDEX_TYPE_UINT32;
}

inline uint32_t vkIndexTypeSize(IndexType t) {
    return t == IndexType::UInt16 ? 2 : 4;
}

}  // namespace Gpu
