#pragma once

#include "framework/gpu/IGpuBuffer.h"

#include <glad/glad.h>

namespace Gpu {

class GlBuffer : public IGpuBuffer {
public:
    explicit GlBuffer(GLuint id) : mId(id) {}
    ~GlBuffer() override;

    GlBuffer(const GlBuffer&) = delete;
    GlBuffer& operator=(const GlBuffer&) = delete;
    GlBuffer(GlBuffer&&) noexcept;
    GlBuffer& operator=(GlBuffer&&) noexcept;

    GLuint id() const { return mId; }

    void write(const void* data, size_t bytes) override;

    void destroy();

private:
    GLuint mId = 0;
};

}  // namespace Gpu
