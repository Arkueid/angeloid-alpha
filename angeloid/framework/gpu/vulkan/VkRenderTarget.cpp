#include "framework/gpu/vulkan/VkRenderTarget.h"
#include "framework/gpu/vulkan/VkDevice.h"
#include "framework/gpu/vulkan/VkTexture.h"
#include "framework/gpu/vulkan/VkTypes.h"

#include "core/util/Log.h"

namespace Gpu {

VkRenderTarget::VkRenderTarget() = default;

VkRenderTarget::~VkRenderTarget() {
    destroy();
}

void VkRenderTarget::init(VulkanDevice* device, int w, int h, bool withColor, bool withDepth) {
    mDevice = device;
    mW = w; mH = h;
    mHasColor = withColor;
    mHasDepth = withDepth;

    if (withColor) {
        mColorTex = std::unique_ptr<VkTexture>(
            new VkTexture(device, w, h, TextureFormat::RGBA8, nullptr));
    }
    if (withDepth) {
        mDepthTex = std::unique_ptr<VkTexture>(
            new VkTexture(device, w, h, TextureFormat::Depth24, nullptr));
    }
}

void VkRenderTarget::bind() {
    if (!mDevice) return;
    mDevice->bindRenderTarget(this);
}

IGpuTexture* VkRenderTarget::colorTexture() {
    return mColorTex.get();
}

IGpuTexture* VkRenderTarget::depthTexture() {
    return mDepthTex.get();
}

VkImageView VkRenderTarget::colorView() const {
    return mColorTex ? mColorTex->imageView() : VK_NULL_HANDLE;
}

VkImageView VkRenderTarget::depthView() const {
    return mDepthTex ? mDepthTex->imageView() : VK_NULL_HANDLE;
}

void VkRenderTarget::destroy() {
    mColorTex.reset();
    mDepthTex.reset();
}

}  // namespace Gpu
