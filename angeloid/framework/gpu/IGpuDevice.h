#pragma once

#include "framework/gpu/Types.h"

#include <memory>
#include <string>
#include <vector>

namespace Gpu {

class IGpuBuffer;
class IGpuTexture;
class IGpuShader;
class IGpuVertexArray;
class IGpuRenderTarget;
class GlDevice;
class VulkanDevice;
struct VertexAttribute;

// ──── IGpuDevice — backend-agnostic GPU device ────
//
//   The single entry point for all GPU operations. Each backend (OpenGL, Vulkan)
//   provides a concrete implementation. Access the current device via
//   Gpu::device() global function.

class IGpuDevice {
public:
    virtual ~IGpuDevice() = default;

    // ── Resource creation ──

    virtual std::unique_ptr<IGpuBuffer> createVertexBuffer(const void* data, size_t bytes,
                                                            BufferUsage usage) = 0;
    virtual std::unique_ptr<IGpuBuffer> createIndexBuffer(const void* data, size_t bytes,
                                                           IndexType type) = 0;
    virtual std::unique_ptr<IGpuTexture> createTexture(int w, int h, TextureFormat fmt,
                                                        const void* data = nullptr) = 0;
    virtual std::unique_ptr<IGpuShader> createShader(const std::string& vertexSrc,
                                                      const std::string& fragmentSrc) = 0;
    virtual std::unique_ptr<IGpuVertexArray>
    createVertexArray(const std::vector<VertexAttribute>& attributes,
                      const std::vector<IGpuBuffer*>& vertexBuffers,
                      IGpuBuffer* indexBuffer, IndexType indexType,
                      int vertexCount, int indexCount) = 0;
    virtual std::unique_ptr<IGpuRenderTarget> createRenderTarget(int w, int h, bool withColor,
                                                                  bool withDepth) = 0;

    // ── State management ──

    virtual void setViewport(int x, int y, int w, int h) = 0;
    virtual void setClearColor(float r, float g, float b, float a) = 0;
    virtual void clear(bool color, bool depth) = 0;

    virtual void setDepthTest(bool enable) = 0;
    virtual void setDepthFunc(CompareFunc func) = 0;

    virtual void setBlend(bool enable) = 0;
    virtual void setBlendFunc(BlendFactor src, BlendFactor dst) = 0;

    virtual void setCullMode(CullMode mode) = 0;
    virtual void setFrontFace(bool clockwise) = 0;  // true=CW (MMD default), false=CCW
    virtual void setPolygonMode(PolygonMode mode) = 0;
    virtual void setPolygonOffset(float factor, float units) = 0;
    virtual void setLineWidth(float width) = 0;

    // ── Framebuffer / screen ──

    virtual void bindScreenFramebuffer(int w, int h) = 0;

    // ── Texture binding ──

    // Explicitly bind a texture to a specific unit slot.
    virtual void bindTextureToUnit(int unit, IGpuTexture* tex) = 0;

    // ── Frame management ──

    // Called once at the start of each frame (before any rendering).
    virtual void beginFrame() = 0;

    // Called once at the end of each frame (after all rendering, submits + presents).
    virtual void endFrame() = 0;

    // OpenGL NDC Z is [-1,1], Vulkan NDC Z is [0,1].
    virtual bool needsDepthCorrection() const { return false; }

    // Backend identification — returns this if matching, nullptr otherwise.
    virtual GlDevice*     asOpenGL()  { return nullptr; }
    virtual VulkanDevice* asVulkan()  { return nullptr; }
};

// ── Global device access ──

// Returns the current GPU device. Must be set via setGpuDevice() before use.
IGpuDevice* device();

}  // namespace Gpu