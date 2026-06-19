#include "framework/gpu/vulkan/VkBuffer.h"
#include "framework/gpu/vulkan/VkDevice.h"
#include "framework/gpu/IGpuDevice.h"

#include "core/util/Log.h"

#include <cstring>

namespace Gpu {

VkBuffer::VkBuffer(VulkanDevice* device, const void* data, size_t bytes,
                   VkBufferUsageFlags usage, bool)
    : mDevice(device), mSize(bytes) {

    VkBufferCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = bytes;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(mDevice->device(), &ci, nullptr, &mBuffer) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create buffer of size %zu", bytes);
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(mDevice->device(), mBuffer, &memReqs);

    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    int memIndex = mDevice->findMemoryType(memReqs.memoryTypeBits, props);
    if (memIndex < 0) {
        MMD_ERROR("VULKAN", "No suitable memory type for buffer");
        destroy();
        return;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = (uint32_t)memIndex;

    if (vkAllocateMemory(mDevice->device(), &allocInfo, nullptr, &mMemory) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to allocate buffer memory (size=%zu)", memReqs.size);
        destroy();
        return;
    }

    vkBindBufferMemory(mDevice->device(), mBuffer, mMemory, 0);
    vkMapMemory(mDevice->device(), mMemory, 0, bytes, 0, &mMapped);
    if (data) memcpy(mMapped, data, bytes);
}

VkBuffer::~VkBuffer() {
    destroy();
}

void VkBuffer::write(const void* data, size_t bytes) {
    if (!mMapped || bytes > mSize) return;
    memcpy(mMapped, data, bytes);
}

void VkBuffer::destroy() {
    if (!Gpu::device()) { mMapped = nullptr; mMemory = VK_NULL_HANDLE; mBuffer = VK_NULL_HANDLE; return; }
    if (mMapped)   { vkUnmapMemory(mDevice->device(), mMemory); mMapped = nullptr; }
    if (mMemory)    { vkFreeMemory(mDevice->device(), mMemory, nullptr); mMemory = VK_NULL_HANDLE; }
    if (mBuffer)    { vkDestroyBuffer(mDevice->device(), mBuffer, nullptr); mBuffer = VK_NULL_HANDLE; }
}

}  // namespace Gpu
