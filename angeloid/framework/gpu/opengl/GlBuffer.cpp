#include "framework/gpu/opengl/GlBuffer.h"

#include <glad/glad.h>

namespace Gpu {

GlBuffer::~GlBuffer() {
    destroy();
}

GlBuffer::GlBuffer(GlBuffer&& other) noexcept : mId(other.mId) {
    other.mId = 0;
}

GlBuffer& GlBuffer::operator=(GlBuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        mId = other.mId;
        other.mId = 0;
    }
    return *this;
}

void GlBuffer::write(const void* data, size_t bytes) {
    glBindBuffer(GL_ARRAY_BUFFER, mId);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_DYNAMIC_DRAW);
}

void GlBuffer::destroy() {
    if (mId) {
        glDeleteBuffers(1, &mId);
        mId = 0;
    }
}

}  // namespace Gpu
