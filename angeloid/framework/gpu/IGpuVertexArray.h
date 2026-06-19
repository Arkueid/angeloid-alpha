#pragma once

#include "framework/gpu/Types.h"

#include <vector>

namespace Gpu {

// Describes one vertex attribute (location, component count, type, stride, offset).
struct VertexAttribute {
    int location;
    int size;  // 1, 2, 3, or 4 components
    DataType dtype;
    int stride;  // 0 = tightly packed
    int offset;
};

class IGpuVertexArray {
public:
    virtual ~IGpuVertexArray() = default;

    // Draw with the given primitive type, vertex count, and first index/vertex.
    // count=-1 means "all vertices/indices in the buffer".
    virtual void draw(PrimitiveType prim, int count = -1, int first = 0) = 0;
};

}  // namespace Gpu