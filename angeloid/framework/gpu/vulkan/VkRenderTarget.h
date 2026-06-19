#pragma once

#include "framework/gpu/IGpuRenderTarget.h"

#include <vulkan/vulkan.h>
#include <memory>

namespace Gpu {

class VulkanDevice;
class VkTexture;

class VkRenderTarget final : public IGpuRenderTarget {
public:
    VkRenderTarget();
    ~VkRenderTarget() override;

    VkRenderTarget(const VkRenderTarget&) = delete;
    VkRenderTarget& operator=(const VkRenderTarget&) = delete;

    // Initialize with the current VulkanDevice
    void init(VulkanDevice* device, int w, int h, bool withColor, bool withDepth);

    void bind() override;
    IGpuTexture* colorTexture() override;
    IGpuTexture* depthTexture() override;

    // For dynamic rendering: the attachment info
    VkImageView colorView() const;
    VkImageView depthView() const;
    int width() const  { return mW; }
    int height() const { return mH; }
    bool hasDepth() const { return mHasDepth; }
    bool hasColor() const { return mHasColor; }

private:
    void destroy();

    VulkanDevice* mDevice = nullptr;
    std::unique_ptr<VkTexture> mColorTex;
    std::unique_ptr<VkTexture> mDepthTex;
    int mW = 0, mH = 0;
    bool mHasColor = false, mHasDepth = false;
};

}  // namespace Gpu
