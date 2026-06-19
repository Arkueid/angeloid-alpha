#pragma once

// ──── Gpu Types — backend-agnostic enums shared by all GPU backends ────

namespace Gpu {

enum class BufferUsage { Static, Dynamic };

enum class IndexType { UInt16, UInt32 };

enum class PrimitiveType { Triangles, Lines };

enum class DataType {
    Float,
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
};

enum class TextureFormat {
    R8,
    RG8,
    RGB8,
    RGBA8,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    Depth24,
};

enum class TextureFilter {
    Nearest,
    Linear,
    LinearMipmapLinear,
};

enum class TextureWrap {
    Clamp,
    Repeat,
    MirrorRepeat,
};

enum class CompareFunc {
    Never,
    Less,
    LEqual,
    Greater,
    GEqual,
    Equal,
    NotEqual,
    Always,
};

enum class BlendFactor {
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    SrcColor,
    OneMinusSrcColor,
    DstAlpha,
    OneMinusDstAlpha,
    DstColor,
    OneMinusDstColor,
};

enum class CullMode {
    None,
    Front,
    Back,
};

enum class PolygonMode {
    Fill,
    Line,
};

}  // namespace Gpu