#include "framework/gpu/opengl/GlDevice.h"

#include "framework/gpu/opengl/GlBuffer.h"
#include "framework/gpu/opengl/GlTexture.h"
#include "framework/gpu/opengl/GlShader.h"
#include "framework/gpu/opengl/GlVertexArray.h"
#include "framework/gpu/opengl/GlRenderTarget.h"
#include "framework/gpu/opengl/GlTypes.h"

#include <glad/glad.h>

#include "core/util/Log.h"

#include <cstdio>

extern "C" int gladLoadGL();

namespace Gpu {

// ── Global device singleton ──

GlDevice::GlDevice() {
    if (!gladLoadGL()) {
        MMD_ERROR("GPU", "Failed to initialize OpenGL (glad)");
    }

    std::printf("OpenGL %d.%d\nRenderer: %s\n",
                GLVersion.major, GLVersion.minor,
                glGetString(GL_RENDERER));

    glEnable(GL_MULTISAMPLE);
}

static std::unique_ptr<IGpuDevice> sDevice;

IGpuDevice* device() {
    return sDevice.get();
}

void setDevice(std::unique_ptr<IGpuDevice> dev) {
    sDevice = std::move(dev);
}

// ── Buffer creation ──

std::unique_ptr<IGpuBuffer> GlDevice::createVertexBuffer(const void* data, size_t bytes,
                                                          BufferUsage usage) {
    GLuint id;
    glGenBuffers(1, &id);
    glBindBuffer(GL_ARRAY_BUFFER, id);
    glBufferData(GL_ARRAY_BUFFER, bytes, data, toGlBufferUsage(usage));
    return std::make_unique<GlBuffer>(id);
}

std::unique_ptr<IGpuBuffer> GlDevice::createIndexBuffer(const void* data, size_t bytes,
                                                         IndexType type) {
    GLuint id;
    glGenBuffers(1, &id);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, bytes, data, GL_STATIC_DRAW);
    auto buf = std::make_unique<GlBuffer>(id);
    // Store index type as a property? We don't have that on IGpuBuffer — the caller
    // passes IndexType to createVertexArray which records it in GlVertexArray.
    (void)type;
    return buf;
}

// ── Texture creation ──

std::unique_ptr<IGpuTexture> GlDevice::createTexture(int w, int h, TextureFormat fmt,
                                                       const void* data) {
    return std::make_unique<GlTexture>(w, h, fmt, data);
}

// ── Shader creation ──

std::unique_ptr<IGpuShader> GlDevice::createShader(const std::string& vertexSrc,
                                                     const std::string& fragmentSrc) {
    return std::make_unique<GlShader>(vertexSrc, fragmentSrc);
}

// ── Vertex array creation ──

std::unique_ptr<IGpuVertexArray>
GlDevice::createVertexArray(const std::vector<VertexAttribute>& attributes,
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

    if (indexBuffer) {
        auto* eb = static_cast<GlBuffer*>(indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eb->id());
    }

    glBindVertexArray(0);

    auto* eb = static_cast<GlBuffer*>(indexBuffer);
    return std::unique_ptr<GlVertexArray>(
        new GlVertexArray(vaoId, {}, eb, indexCount, vertexCount, indexType));
}

// ── Render target creation ──

std::unique_ptr<IGpuRenderTarget> GlDevice::createRenderTarget(int w, int h, bool withColor,
                                                                bool withDepth) {
    auto rt = std::make_unique<GlRenderTarget>();
    rt->resize(w, h, withColor, withDepth);
    return rt;
}

// ── State management ──

void GlDevice::setViewport(int x, int y, int w, int h) {
    glViewport(x, y, w, h);
}

void GlDevice::setClearColor(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
}

void GlDevice::clear(bool color, bool depth) {
    GLbitfield mask = 0;
    if (color) mask |= GL_COLOR_BUFFER_BIT;
    if (depth) mask |= GL_DEPTH_BUFFER_BIT;
    if (mask) glClear(mask);
}

void GlDevice::setDepthTest(bool enable) {
    if (enable) glEnable(GL_DEPTH_TEST);
    else        glDisable(GL_DEPTH_TEST);
}

void GlDevice::setDepthFunc(CompareFunc func) {
    glDepthFunc(toGlCompareFunc(func));
}

void GlDevice::setBlend(bool enable) {
    if (enable) glEnable(GL_BLEND);
    else        glDisable(GL_BLEND);
}

void GlDevice::setBlendFunc(BlendFactor src, BlendFactor dst) {
    glBlendFunc(toGlBlendFactor(src), toGlBlendFactor(dst));
}

void GlDevice::setCullMode(CullMode mode) {
    if (mode == CullMode::None) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
        glCullFace(toGlCullMode(mode));
    }
}

void GlDevice::setFrontFace(bool clockwise) {
    glFrontFace(clockwise ? GL_CW : GL_CCW);
}

void GlDevice::setPolygonMode(PolygonMode mode) {
    glPolygonMode(GL_FRONT_AND_BACK, toGlPolygonMode(mode));
}

void GlDevice::setPolygonOffset(float factor, float units) {
    if (factor == 0.0f && units == 0.0f) {
        glDisable(GL_POLYGON_OFFSET_LINE);
    } else {
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(factor, units);
    }
}

void GlDevice::setLineWidth(float width) {
    glLineWidth(width);
}

// ── Screen ──

void GlDevice::bindScreenFramebuffer(int w, int h) {
    GlRenderTarget::bindScreen(w, h);
}

// ── Texture binding ──

void GlDevice::bindTextureToUnit(int unit, IGpuTexture* tex) {
    if (tex) {
        tex->bind(unit);
    }
}

}  // namespace Gpu
