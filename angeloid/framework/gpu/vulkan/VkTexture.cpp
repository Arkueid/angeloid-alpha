#include "framework/gpu/vulkan/VkTexture.h"
#include "framework/gpu/vulkan/VkDevice.h"
#include "framework/gpu/vulkan/VkTypes.h"
#include "framework/gpu/IGpuDevice.h"

#include "core/util/Log.h"

#include <cstring>

namespace Gpu {

VkTexture::VkTexture(VulkanDevice* device, int w, int h, TextureFormat fmt, const void* data)
    : mDevice(device), mWidth(w), mHeight(h), mFormat(fmt) {

    // Create image
    VkImageCreateInfo imgCI{};
    imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCI.imageType = VK_IMAGE_TYPE_2D;
    imgCI.format = toVkFormat(fmt);
    imgCI.extent = {(uint32_t)w, (uint32_t)h, 1};
    imgCI.mipLevels = 1;
    imgCI.arrayLayers = 1;
    imgCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    bool depth = (fmt == TextureFormat::Depth24);
    VkImageUsageFlags vkUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (depth) {
        vkUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        // Don't upload depth textures — they're cleared by the render pass
        data = nullptr;
    }

    imgCI.usage = vkUsage;
    imgCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(mDevice->device(), &imgCI, nullptr, &mImage) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create image %dx%d", w, h);
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mDevice->device(), mImage, &memReqs);

    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    int memIndex = mDevice->findMemoryType(memReqs.memoryTypeBits, props);
    if (memIndex < 0) {
        MMD_ERROR("VULKAN", "No device-local memory for image");
        destroy();
        return;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = (uint32_t)memIndex;

    if (vkAllocateMemory(mDevice->device(), &allocInfo, nullptr, &mMemory) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to allocate image memory");
        destroy();
        return;
    }
    vkBindImageMemory(mDevice->device(), mImage, mMemory, 0);

    // Image view
    VkImageViewCreateInfo viewCI{};
    viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image = mImage;
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = toVkFormat(fmt);
    viewCI.subresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.layerCount = 1;

    if (vkCreateImageView(mDevice->device(), &viewCI, nullptr, &mImageView) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create image view");
        destroy();
        return;
    }

    // Default sampler (not used for depth textures, but created for interface consistency)
    createSampler();

    // Upload initial data via staging
    if (data) {
        size_t dataSize = (size_t)w * h * vkFormatSize(fmt);
        mDevice->uploadImage(mImage, w, h, dataSize, data);
        mLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    } else if (depth) {
        mLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // Will be set by render pass
    } else {
        mLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

VkTexture::~VkTexture() {
    destroy();
}

void VkTexture::createSampler(bool compareEnable) {
    if (mSampler) vkDestroySampler(mDevice->device(), mSampler, nullptr);

    VkSamplerCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_LINEAR;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.maxLod = 1.0f;
    ci.compareEnable = compareEnable ? VK_TRUE : VK_FALSE;
    ci.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    if (vkCreateSampler(mDevice->device(), &ci, nullptr, &mSampler) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create sampler");
    }
}

void VkTexture::enableCompare() {
    createSampler(true);
}

void VkTexture::setFilter(TextureFilter minFilter, TextureFilter magFilter) {
    if (mSampler) {
        vkDestroySampler(mDevice->device(), mSampler, nullptr);
        mSampler = VK_NULL_HANDLE;
    }

    VkSamplerCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = toVkFilter(magFilter);
    ci.minFilter = toVkFilter(minFilter);
    ci.mipmapMode = toVkSamplerMipmapMode(minFilter);
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.maxLod = 1.0f;

    if (vkCreateSampler(mDevice->device(), &ci, nullptr, &mSampler) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to recreate sampler");
    }
}

void VkTexture::setWrap(TextureWrap s, TextureWrap t) {
    if (mSampler) {
        vkDestroySampler(mDevice->device(), mSampler, nullptr);
        mSampler = VK_NULL_HANDLE;
    }

    VkSamplerCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_LINEAR;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.addressModeU = toVkSamplerAddressMode(s);
    ci.addressModeV = toVkSamplerAddressMode(t);
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.maxLod = 1.0f;

    if (vkCreateSampler(mDevice->device(), &ci, nullptr, &mSampler) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to recreate sampler");
    }
}

void VkTexture::setMirrorWrap(bool mirrorX, bool mirrorY) {
    if (mSampler) {
        vkDestroySampler(mDevice->device(), mSampler, nullptr);
        mSampler = VK_NULL_HANDLE;
    }

    VkSamplerCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_LINEAR;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.addressModeU = mirrorX ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                              : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeV = mirrorY ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                              : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.maxLod = 1.0f;

    if (vkCreateSampler(mDevice->device(), &ci, nullptr, &mSampler) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to recreate sampler");
    }
}

void VkTexture::bind(int unit) {
    mBoundUnit = unit;
    // Register with device so flushDescriptorSet picks up this binding
    mDevice->bindTextureToUnit(unit, this);
}

void VkTexture::write(const void* data) {
    if (!data || !mImage) return;
    size_t dataSize = (size_t)mWidth * mHeight * vkFormatSize(mFormat);
    mDevice->uploadImage(mImage, mWidth, mHeight, dataSize, data);
}

void VkTexture::destroy() {
    if (!Gpu::device()) { mSampler = VK_NULL_HANDLE; mImageView = VK_NULL_HANDLE; mMemory = VK_NULL_HANDLE; mImage = VK_NULL_HANDLE; return; }
    if (mSampler) {
        vkDestroySampler(mDevice->device(), mSampler, nullptr);
        mSampler = VK_NULL_HANDLE;
    }
    if (mImageView) {
        vkDestroyImageView(mDevice->device(), mImageView, nullptr);
        mImageView = VK_NULL_HANDLE;
    }
    if (mMemory) {
        vkFreeMemory(mDevice->device(), mMemory, nullptr);
        mMemory = VK_NULL_HANDLE;
    }
    if (mImage) {
        vkDestroyImage(mDevice->device(), mImage, nullptr);
        mImage = VK_NULL_HANDLE;
    }
}

}  // namespace Gpu
