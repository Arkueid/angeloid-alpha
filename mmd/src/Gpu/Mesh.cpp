#include "Gpu/Mesh.h"

#include <cstring>

namespace Gpu {

// --- VboWrapper ---

void VboWrapper::write(const void* data, size_t bytes) const
{
    glBindBuffer(GL_ARRAY_BUFFER, vboId);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_DYNAMIC_DRAW);
}

// --- Vao ---

Vao::Vao()
{
    glGenVertexArrays(1, &vaoId);
}

Vao::~Vao()
{
    destroy();
}

Vao::Vao(Vao&& other) noexcept
    : vaoId(other.vaoId)
    , vbos(std::move(other.vbos))
    , ebo(other.ebo)
    , indexCount(other.indexCount)
    , vertexCount(other.vertexCount)
{
    other.vaoId = 0;
    other.ebo = 0;
    other.indexCount = 0;
    other.vertexCount = 0;
}

Vao& Vao::operator=(Vao&& other) noexcept
{
    if (this != &other) {
        destroy();
        vaoId = other.vaoId; other.vaoId = 0;
        vbos = std::move(other.vbos);
        ebo = other.ebo; other.ebo = 0;
        indexCount = other.indexCount; other.indexCount = 0;
        vertexCount = other.vertexCount; other.vertexCount = 0;
    }
    return *this;
}

void Vao::bind() const { glBindVertexArray(vaoId); }
void Vao::unbind() { glBindVertexArray(0); }

GLuint Vao::addVbo(const void* data, size_t bytes, int location, int size,
                   GLenum dtype, int stride, int offset)
{
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(location);
    if (dtype == GL_INT || dtype == GL_UNSIGNED_INT ||
        dtype == GL_SHORT || dtype == GL_UNSIGNED_SHORT ||
        dtype == GL_BYTE || dtype == GL_UNSIGNED_BYTE) {
        glVertexAttribIPointer(location, size, dtype, stride, (const void*)(intptr_t)offset);
    } else {
        glVertexAttribPointer(location, size, dtype, GL_FALSE, stride, (const void*)(intptr_t)offset);
    }
    vbos.push_back(vbo);
    return vbo;
}

void Vao::setEbo(const void* data, size_t bytes, GLenum indexType)
{
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
    if (indexType == GL_UNSIGNED_INT) {
        indexCount = (int)(bytes / 4);
    } else if (indexType == GL_UNSIGNED_SHORT) {
        indexCount = (int)(bytes / 2);
    } else {
        indexCount = (int)bytes;
    }
}

void Vao::updateVbo(int vboIndex, const void* data, size_t bytes) const
{
    glBindBuffer(GL_ARRAY_BUFFER, vbos[vboIndex]);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, GL_DYNAMIC_DRAW);
}

void Vao::render(GLenum mode, int count, int first) const
{
    bind();
    if (ebo) {
        int n = count >= 0 ? count : indexCount;
        glDrawElements(mode, n, GL_UNSIGNED_INT, (const void*)(intptr_t)(first * 4));
    } else {
        int n = count >= 0 ? count : vertexCount;
        glDrawArrays(mode, first, n);
    }
    unbind();
}

void Vao::destroy()
{
    if (!vbos.empty()) {
        glDeleteBuffers((GLsizei)vbos.size(), vbos.data());
        vbos.clear();
    }
    if (ebo) {
        glDeleteBuffers(1, &ebo);
        ebo = 0;
    }
    if (vaoId) {
        glDeleteVertexArrays(1, &vaoId);
        vaoId = 0;
    }
}

// --- Gpu::Vao::create ---

Vao Vao::create(const std::vector<VertexBufferDesc>& vertexBuffers,
                          const void* indices, size_t indexBytes,
                          GLenum indexType)
{
    Vao vao;
    vao.bind();
    for (const auto& desc : vertexBuffers) {
        GLuint vbo;
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, desc.bytes, desc.data, GL_STATIC_DRAW);
        glEnableVertexAttribArray(desc.location);
        if (desc.dtype == GL_INT || desc.dtype == GL_UNSIGNED_INT ||
            desc.dtype == GL_SHORT || desc.dtype == GL_UNSIGNED_SHORT ||
            desc.dtype == GL_BYTE || desc.dtype == GL_UNSIGNED_BYTE) {
            glVertexAttribIPointer(desc.location, desc.size, desc.dtype, 0, nullptr);
        } else {
            glVertexAttribPointer(desc.location, desc.size, desc.dtype, GL_FALSE, 0, nullptr);
        }
        vao.vbos.push_back(vbo);
    }
    vao.setEbo(indices, indexBytes, indexType);
    Vao::unbind();
    return vao;
}

} // namespace Gpu
