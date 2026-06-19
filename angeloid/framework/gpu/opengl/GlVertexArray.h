#pragma once

#include "framework/gpu/IGpuVertexArray.h"
#include "framework/gpu/opengl/GlBuffer.h"

#include <glad/glad.h>
#include <memory>
#include <vector>

namespace Gpu {

class GlVertexArray : public IGpuVertexArray {
public:
    GlVertexArray() = default;
    explicit GlVertexArray(GLuint vaoId, std::vector<std::unique_ptr<GlBuffer>> buffers,
                           GlBuffer* ebo, int indexCount, int vertexCount,
                           IndexType indexType);
    ~GlVertexArray() override;

    GlVertexArray(const GlVertexArray&) = delete;
    GlVertexArray& operator=(const GlVertexArray&) = delete;
    GlVertexArray(GlVertexArray&&) noexcept;
    GlVertexArray& operator=(GlVertexArray&&) noexcept;

    GLuint id() const { return mVaoId; }

    void draw(PrimitiveType prim, int count = -1, int first = 0) override;

    // Public vertex count for callers that need it (e.g. empty checks).
    int vertexCount = 0;

    // Register a buffer as owned by this VAO (for lifecycle management).
    void addOwnedBuffer(std::unique_ptr<GlBuffer> buf);

    // Access owned buffers (e.g. for morph VBO wrapping).
    GlBuffer* ownedBuffer(size_t index) const;

    void destroy();

    static std::unique_ptr<GlVertexArray>
    create(const std::vector<VertexAttribute>& attributes,
           const std::vector<IGpuBuffer*>& vertexBuffers,
           IGpuBuffer* indexBuffer, IndexType indexType, int vertexCount, int indexCount);

private:
    GLuint mVaoId = 0;
    std::vector<std::unique_ptr<GlBuffer>> mOwnedBuffers;
    GLuint mEbo = 0;
    int mIndexCount = 0;
    IndexType mIndexType = IndexType::UInt32;
    bool mOwnsEbo = false;
};

}  // namespace Gpu
