#pragma once

#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/IGpuBuffer.h"
#include "framework/gpu/IGpuTexture.h"
#include "framework/gpu/IGpuShader.h"
#include "framework/gpu/IGpuVertexArray.h"
#include "framework/gpu/IGpuRenderTarget.h"

#include <memory>
#include <vector>

namespace Gpu {

class GlDevice : public IGpuDevice {
public:
    GlDevice();
    ~GlDevice() override = default;

    // Resource creation
    std::unique_ptr<IGpuBuffer> createVertexBuffer(const void* data, size_t bytes,
                                                    BufferUsage usage) override;
    std::unique_ptr<IGpuBuffer> createIndexBuffer(const void* data, size_t bytes,
                                                   IndexType type) override;
    std::unique_ptr<IGpuTexture> createTexture(int w, int h, TextureFormat fmt,
                                                const void* data = nullptr) override;
    std::unique_ptr<IGpuShader> createShader(const std::string& vertexSrc,
                                              const std::string& fragmentSrc) override;
    std::unique_ptr<IGpuVertexArray>
    createVertexArray(const std::vector<VertexAttribute>& attributes,
                      const std::vector<IGpuBuffer*>& vertexBuffers,
                      IGpuBuffer* indexBuffer, IndexType indexType,
                      int vertexCount, int indexCount) override;
    std::unique_ptr<IGpuRenderTarget> createRenderTarget(int w, int h, bool withColor,
                                                          bool withDepth) override;

    // State management
    void setViewport(int x, int y, int w, int h) override;
    void setClearColor(float r, float g, float b, float a) override;
    void clear(bool color, bool depth) override;
    void setDepthTest(bool enable) override;
    void setDepthFunc(CompareFunc func) override;
    void setBlend(bool enable) override;
    void setBlendFunc(BlendFactor src, BlendFactor dst) override;
    void setCullMode(CullMode mode) override;
    void setFrontFace(bool clockwise) override;
    void setPolygonMode(PolygonMode mode) override;
    void setPolygonOffset(float factor, float units) override;
    void setLineWidth(float width) override;

    // Screen
    void bindScreenFramebuffer(int w, int h) override;

    // Texture binding
    void bindTextureToUnit(int unit, IGpuTexture* tex) override;

    // Frame management (no-ops for OpenGL)
    void beginFrame() override {}
    void endFrame() override {}

    GlDevice* asOpenGL() override { return this; }
};

// Set the global GPU device (called once at init).
void setDevice(std::unique_ptr<IGpuDevice> dev);

}  // namespace Gpu
