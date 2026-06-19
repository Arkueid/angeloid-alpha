#include "framework/gpu/opengl/GlVertexArray.h"
#include "framework/gpu/opengl/GlTypes.h"

namespace Gpu {

GlVertexArray::GlVertexArray(GLuint vaoId, std::vector<std::unique_ptr<GlBuffer>> buffers,
                             GlBuffer* ebo, int indexCount, int vertexCount,
                             IndexType indexType)
    : mVaoId(vaoId), mOwnedBuffers(std::move(buffers)),
      mIndexCount(indexCount), vertexCount(vertexCount), mIndexType(indexType) {
    if (ebo) {
        mEbo = ebo->id();
    }
}

GlVertexArray::~GlVertexArray() {
    destroy();
}

GlVertexArray::GlVertexArray(GlVertexArray&& other) noexcept
    : mVaoId(other.mVaoId),
      mOwnedBuffers(std::move(other.mOwnedBuffers)),
      mEbo(other.mEbo),
      mIndexCount(other.mIndexCount),
      vertexCount(other.vertexCount),
      mIndexType(other.mIndexType),
      mOwnsEbo(other.mOwnsEbo) {
    other.mVaoId = 0;
    other.mEbo = 0;
    other.mIndexCount = 0;
    other.vertexCount = 0;
    other.mOwnsEbo = false;
}

GlVertexArray& GlVertexArray::operator=(GlVertexArray&& other) noexcept {
    if (this != &other) {
        destroy();
        mVaoId = other.mVaoId; other.mVaoId = 0;
        mOwnedBuffers = std::move(other.mOwnedBuffers);
        mEbo = other.mEbo; other.mEbo = 0;
        mIndexCount = other.mIndexCount; other.mIndexCount = 0;
        vertexCount = other.vertexCount; other.vertexCount = 0;
        mIndexType = other.mIndexType;
        mOwnsEbo = other.mOwnsEbo; other.mOwnsEbo = false;
    }
    return *this;
}

void GlVertexArray::draw(PrimitiveType prim, int count, int first) {
    glBindVertexArray(mVaoId);
    if (mEbo) {
        int n = count >= 0 ? count : mIndexCount;
        glDrawElements(toGlPrimitive(prim), n, toGlIndexType(mIndexType),
                       (const void*)(intptr_t)(first * toGlIndexBytes(mIndexType)));
    } else {
        int n = count >= 0 ? count : vertexCount;
        glDrawArrays(toGlPrimitive(prim), first, n);
    }
    glBindVertexArray(0);
}

void GlVertexArray::addOwnedBuffer(std::unique_ptr<GlBuffer> buf) {
    mOwnedBuffers.push_back(std::move(buf));
}

GlBuffer* GlVertexArray::ownedBuffer(size_t index) const {
    return index < mOwnedBuffers.size() ? mOwnedBuffers[index].get() : nullptr;
}

void GlVertexArray::destroy() {
    if (mVaoId) {
        glDeleteVertexArrays(1, &mVaoId);
        mVaoId = 0;
    }
    mOwnedBuffers.clear();
    mEbo = 0;
    mOwnsEbo = false;
}

// --- Factory ---

std::unique_ptr<GlVertexArray>
GlVertexArray::create(const std::vector<VertexAttribute>& attributes,
                      const std::vector<IGpuBuffer*>& vertexBuffers,
                      IGpuBuffer* indexBuffer, IndexType indexType,
                      int vertexCount, int indexCount) {
    GLuint vaoId;
    glGenVertexArrays(1, &vaoId);
    glBindVertexArray(vaoId);

    for (size_t i = 0; i < attributes.size(); ++i) {
        const auto& attr = attributes[i];
        auto* buf = static_cast<GlBuffer*>(vertexBuffers[i]);
        glBindBuffer(GL_ARRAY_BUFFER, buf->id());
        glEnableVertexAttribArray(attr.location);
        GLenum dtype = toGlDataType(attr.dtype);
        int stride = attr.stride > 0 ? attr.stride : 0;
        if (isIntegerDataType(attr.dtype)) {
            glVertexAttribIPointer(attr.location, attr.size, dtype, stride,
                                   (const void*)(intptr_t)attr.offset);
        } else {
            glVertexAttribPointer(attr.location, attr.size, dtype, GL_FALSE, stride,
                                  (const void*)(intptr_t)attr.offset);
        }
    }

    GLuint eboId = 0;
    if (indexBuffer) {
        auto* eb = static_cast<GlBuffer*>(indexBuffer);
        eboId = eb->id();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboId);
    }

    glBindVertexArray(0);

    auto* eb = static_cast<GlBuffer*>(indexBuffer);
    return std::unique_ptr<GlVertexArray>(
        new GlVertexArray(vaoId, {}, eb, indexCount, vertexCount, indexType));
}

}  // namespace Gpu
