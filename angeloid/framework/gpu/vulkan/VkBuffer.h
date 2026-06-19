#pragma once

#include "framework/gpu/IGpuBuffer.h"

#include <vulkan/vulkan.h>
#include <cstddef>

namespace Gpu {

class VulkanDevice;

class VkBuffer final : public IGpuBuffer {
public:
    VkBuffer(VulkanDevice* device, const void* data, size_t bytes,
             VkBufferUsageFlags usage, bool cpuWritable);
    ~VkBuffer() override;

    VkBuffer(const VkBuffer&) = delete;
    VkBuffer& operator=(const VkBuffer&) = delete;

    ::VkBuffer buffer() const { return mBuffer; }
    size_t size() const { return mSize; }

    void write(const void* data, size_t bytes) override;

private:
    void destroy();

    VulkanDevice* mDevice;
    ::VkBuffer mBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;
    void* mMapped = nullptr;
    size_t mSize = 0;
};

}  // namespace Gpu
