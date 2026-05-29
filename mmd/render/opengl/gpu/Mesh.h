#pragma once

#include <GL/glew.h>
#include <cstdint>
#include <vector>

namespace Gpu {

struct VboWrapper {
    GLuint vboId = 0;
    explicit VboWrapper(GLuint id) : vboId(id)
    {
    }
    void write(const void* data, size_t bytes) const;
};

struct VertexBufferDesc {
    int location;
    const void* data;
    size_t bytes;
    int size;
    GLenum dtype = GL_FLOAT;
};

struct Vao {
    GLuint vaoId = 0;
    std::vector<GLuint> vbos;
    GLuint ebo = 0;
    int indexCount = 0;
    int vertexCount = 0;

    Vao();
    ~Vao();
    Vao(const Vao&) = delete;
    Vao& operator=(const Vao&) = delete;
    Vao(Vao&&) noexcept;
    Vao& operator=(Vao&&) noexcept;

    void bind() const;
    static void unbind();

    GLuint addVbo(const void* data, size_t bytes, int location, int size, GLenum dtype = GL_FLOAT,
                  int stride = 0, int offset = 0);
    void setEbo(const void* data, size_t bytes, GLenum indexType = GL_UNSIGNED_INT);
    void updateVbo(int vboIndex, const void* data, size_t bytes) const;
    void render(GLenum mode = GL_TRIANGLES, int count = -1, int first = 0) const;
    void destroy();

    static Vao create(const std::vector<VertexBufferDesc>& vertexBuffers, const void* indices,
                      size_t indexBytes, GLenum indexType = GL_UNSIGNED_INT);
};

}  // namespace Gpu
