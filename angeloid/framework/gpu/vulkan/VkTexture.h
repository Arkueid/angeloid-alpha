#pragma once

#include "framework/gpu/IGpuTexture.h"
#include "framework/gpu/Types.h"

#include <vulkan/vulkan.h>

namespace Gpu {

class VulkanDevice;

class VkTexture final : public IGpuTexture {
public:
    VkTexture(VulkanDevice* device, int w, int h, TextureFormat fmt, const void* data);
    ~VkTexture() override;

    VkTexture(const VkTexture&) = delete;
    VkTexture& operator=(const VkTexture&) = delete;

    int width() const override  { return mWidth; }
    int height() const override { return mHeight; }

    void bind(int unit) override;
    void setFilter(TextureFilter minFilter, TextureFilter magFilter) override;
    void setWrap(TextureWrap s, TextureWrap t) override;
    void setMirrorWrap(bool mirrorX, bool mirrorY) override;
    void write(const void* data) override;

    VkImage image() const { return mImage; }
    VkImageView imageView() const { return mImageView; }
    VkSampler sampler() const { return mSampler; }
    VkImageLayout currentLayout() const { return mLayout; }
    void setLayout(VkImageLayout layout) { mLayout = layout; }
    bool isDepth() const { return mFormat == TextureFormat::Depth24; }

    // Enable depth comparison for shadow map sampling (recreates sampler).
    void enableCompare();

private:
    void createSampler(bool compareEnable = false);
    void destroy();

    VulkanDevice* mDevice;
    int mWidth = 0, mHeight = 0;
    TextureFormat mFormat = TextureFormat::RGBA8;

    VkImage mImage = VK_NULL_HANDLE;
    VkDeviceMemory mMemory = VK_NULL_HANDLE;
    VkImageView mImageView = VK_NULL_HANDLE;
    VkSampler mSampler = VK_NULL_HANDLE;
    VkImageLayout mLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    int mBoundUnit = -1;
};

}  // namespace Gpu
