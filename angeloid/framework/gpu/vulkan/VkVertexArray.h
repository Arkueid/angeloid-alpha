#pragma once

#include "framework/gpu/IGpuVertexArray.h"
#include "framework/gpu/Types.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace Gpu {

class IGpuBuffer;
class VulkanDevice;
class VkBuffer;

class VkVertexArray final : public IGpuVertexArray {
public:
    VkVertexArray(VulkanDevice* device,
                  const std::vector<VertexAttribute>& attributes,
                  const std::vector<IGpuBuffer*>& vertexBuffers,
                  IGpuBuffer* indexBuffer,
                  IndexType indexType,
                  int vertexCount,
                  int indexCount);
    ~VkVertexArray() override;

    VkVertexArray(const VkVertexArray&) = delete;
    VkVertexArray& operator=(const VkVertexArray&) = delete;

    void draw(PrimitiveType prim, int count = -1, int first = 0) override;

    // For pipeline creation: returns the vertex input state
    const std::vector<VkVertexInputBindingDescription>& bindings() const { return mBindings; }
    const std::vector<VkVertexInputAttributeDescription>& attributes() const { return mAttribs; }
    int vertexCount() const { return mVertexCount; }
    int indexCount() const { return mIndexCount; }
    IndexType indexType() const { return mIndexType; }
    bool hasIndexBuffer() const { return mHasIndexBuffer; }

    // Per-frame buffer references for draw
    const std::vector<::VkBuffer>& vkBuffers() const { return mVkBuffers; }
    ::VkBuffer vkIndexBuffer() const { return mVkIndexBuffer; }

private:
    VulkanDevice* mDevice;
    std::vector<VkVertexInputBindingDescription> mBindings;
    std::vector<VkVertexInputAttributeDescription> mAttribs;

    // Vulkan buffer handles for per-draw binding
    std::vector<::VkBuffer> mVkBuffers;
    ::VkBuffer mVkIndexBuffer = VK_NULL_HANDLE;

    int mVertexCount = 0;
    int mIndexCount = 0;
    IndexType mIndexType = IndexType::UInt32;
    bool mHasIndexBuffer = false;
};

}  // namespace Gpu
