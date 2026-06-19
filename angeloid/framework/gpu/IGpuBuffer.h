#pragma once

#include <cstddef>

namespace Gpu {

class IGpuBuffer {
public:
    virtual ~IGpuBuffer() = default;

    // Upload data to the entire buffer (re-allocates).
    virtual void write(const void* data, size_t bytes) = 0;
};

}  // namespace Gpu