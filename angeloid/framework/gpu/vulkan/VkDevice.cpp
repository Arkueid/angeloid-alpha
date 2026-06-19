#include "framework/gpu/vulkan/VkDevice.h"
#include "framework/gpu/vulkan/VkBuffer.h"
#include "framework/gpu/vulkan/VkTexture.h"
#include "framework/gpu/vulkan/VkShader.h"
#include "framework/gpu/vulkan/VkVertexArray.h"
#include "framework/gpu/vulkan/VkRenderTarget.h"
#include "framework/gpu/vulkan/VkTypes.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "core/util/Log.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

namespace Gpu {

// ──── Helpers ────

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
};

#ifdef _DEBUG
static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};
static const bool kEnableValidation = true;
#else
static const std::vector<const char*> kValidationLayers = {};
static const bool kEnableValidation = false;
#endif

static bool checkValidationLayerSupport() {
    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());

    for (const char* name : kValidationLayers) {
        bool found = false;
        for (const auto& layer : available) {
            if (strcmp(name, layer.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

// ──── VulkanDevice ────

VulkanDevice::VulkanDevice(GLFWwindow* window) : mWindow(window) {
    createInstance();
    createSurface();
    selectPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createSwapchain();
    createPerFrameResources(3);
    createSyncObjects();
    createDescriptorPool();
    createUniformRing();
    createDummyTexture();

    VkPipelineCacheCreateInfo cacheCI{};
    cacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    vkCreatePipelineCache(mDevice, &cacheCI, nullptr, &mPipelineCacheVk);

    MMD_INFO("VULKAN", "Device: %s", mDeviceProps.deviceName);
}

VulkanDevice::~VulkanDevice() {
    vkDeviceWaitIdle(mDevice);

    // Destroy pipeline cache
    for (auto& [key, pipeline] : mPipelineCache) {
        vkDestroyPipeline(mDevice, pipeline, nullptr);
    }
    mPipelineCache.clear();
    if (mPipelineCacheVk) {
        vkDestroyPipelineCache(mDevice, mPipelineCacheVk, nullptr);
        mPipelineCacheVk = VK_NULL_HANDLE;
    }

    mShaderDescSets.clear();
    mDummyTexture.reset();

    if (mDescPool) {
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
        mDescPool = VK_NULL_HANDLE;
    }

    if (mUniformRingBuffer) {
        vkUnmapMemory(mDevice, mUniformRingMemory);
        vkDestroyBuffer(mDevice, mUniformRingBuffer, nullptr);
        vkFreeMemory(mDevice, mUniformRingMemory, nullptr);
    }

    cleanupSwapchain();

    for (uint32_t i = 0; i < mFrameCount; ++i) {
        if (mFrameFences[i]) vkDestroyFence(mDevice, mFrameFences[i], nullptr);
        if (mImageAvailableSemaphores[i]) vkDestroySemaphore(mDevice, mImageAvailableSemaphores[i], nullptr);
        if (mRenderFinishedSemaphores[i]) vkDestroySemaphore(mDevice, mRenderFinishedSemaphores[i], nullptr);
    }

    if (mCmdPool) {
        vkDestroyCommandPool(mDevice, mCmdPool, nullptr);
    }
    if (mDevice) {
        vkDestroyDevice(mDevice, nullptr);
    }
    if (mSurface) {
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    }
    if (mInstance) {
        vkDestroyInstance(mInstance, nullptr);
    }
}

// ──── Instance / Device ────

void VulkanDevice::createInstance() {
    if (kEnableValidation && !checkValidationLayerSupport()) {
        MMD_WARN("VULKAN", "Validation layers requested but not available");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Angeloid Alpha";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "angeloid";
    appInfo.apiVersion = VK_API_VERSION_1_2;

    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = (uint32_t)extensions.size();
    ci.ppEnabledExtensionNames = extensions.data();
    ci.enabledLayerCount = (uint32_t)kValidationLayers.size();
    ci.ppEnabledLayerNames = kValidationLayers.data();

    if (vkCreateInstance(&ci, nullptr, &mInstance) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create Vulkan instance");
    }
}

void VulkanDevice::createSurface() {
    if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create window surface");
    }
}

void VulkanDevice::selectPhysicalDevice() {
    uint32_t count;
    vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(mInstance, &count, devices.data());

    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        // Prefer discrete GPU
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            mPhysicalDevice = dev;
            mDeviceProps = props;
            break;
        }
    }

    if (!mPhysicalDevice && !devices.empty()) {
        mPhysicalDevice = devices[0];
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &mDeviceProps);
    }

    if (!mPhysicalDevice) {
        MMD_ERROR("VULKAN", "No suitable Vulkan physical device found");
        return;
    }

    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mMemoryProps);

    // Check MSAA support
    VkSampleCountFlags counts = mDeviceProps.limits.framebufferColorSampleCounts
                              & mDeviceProps.limits.framebufferDepthSampleCounts;
    if (!(counts & VK_SAMPLE_COUNT_4_BIT)) {
        mMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
        MMD_INFO("VULKAN", "MSAA 4x not supported, falling back to 1x");
    }

    // Find graphics queue family
    uint32_t qfCount;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &qfCount, qfProps.data());

    for (uint32_t i = 0; i < qfCount; ++i) {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &presentSupport);
        if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport) {
            mGraphicsQueueFamily = i;
            break;
        }
    }
}

void VulkanDevice::createLogicalDevice() {
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo qCI{};
    qCI.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qCI.queueFamilyIndex = mGraphicsQueueFamily;
    qCI.queueCount = 1;
    qCI.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceDynamicRenderingFeatures dynRender{};
    dynRender.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynRender.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.samplerAnisotropy = VK_FALSE;
    features2.features.fillModeNonSolid = VK_TRUE;
    features2.features.wideLines = VK_TRUE;
    features2.pNext = &dynRender;

    VkDeviceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pNext = &features2;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qCI;
    ci.enabledExtensionCount = (uint32_t)kDeviceExtensions.size();
    ci.ppEnabledExtensionNames = kDeviceExtensions.data();

    if (vkCreateDevice(mPhysicalDevice, &ci, nullptr, &mDevice) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create logical device");
        return;
    }

    vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
}

// ──── Swapchain ────

void VulkanDevice::createMsImage(uint32_t w, uint32_t h, VkFormat fmt,
                                  VkImageUsageFlags usage, VkImageAspectFlags aspect,
                                  VkImageLayout targetLayout,
                                  VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = fmt;
    ici.extent = {w, h, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = mMsaaSamples;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = usage;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCreateImage(mDevice, &ici, nullptr, &img);

    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(mDevice, img, &mr);
    int mi = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = (uint32_t)mi;
    vkAllocateMemory(mDevice, &ai, nullptr, &mem);
    vkBindImageMemory(mDevice, img, mem, 0);

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = img;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = fmt;
    vci.subresourceRange.aspectMask = aspect;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.layerCount = 1;
    vkCreateImageView(mDevice, &vci, nullptr, &view);

    VkCommandBufferAllocateInfo cai{};
    cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = mCmdPool;
    cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cai.commandBufferCount = 1;
    VkCommandBuffer tmp;
    vkAllocateCommandBuffers(mDevice, &cai, &tmp);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tmp, &bi);
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = targetLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    VkPipelineStageFlags dstStage = (aspect == VK_IMAGE_ASPECT_COLOR_BIT)
        ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        : VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    barrier.dstAccessMask = (aspect == VK_IMAGE_ASPECT_COLOR_BIT)
        ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    vkCmdPipelineBarrier(tmp, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(tmp);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &tmp;
    vkQueueSubmit(mGraphicsQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(mGraphicsQueue);
    vkFreeCommandBuffers(mDevice, mCmdPool, 1, &tmp);
}

void VulkanDevice::createScreenDepth() {
    if (mSwapchainExtent.width == 0 || mSwapchainExtent.height == 0) return;
    auto w = mSwapchainExtent.width, h = mSwapchainExtent.height;
    createMsImage(w, h, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                  VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  mScreenColorMS, mScreenColorMSMemory, mScreenColorMSView);
    createMsImage(w, h, VK_FORMAT_D32_SFLOAT,
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                  VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                  mScreenDepthMS, mScreenDepthMSMemory, mScreenDepthMSView);
}

void VulkanDevice::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

    VkExtent2D extent;
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        int w, h;
        glfwGetFramebufferSize(mWindow, &w, &h);
        extent = {(uint32_t)w, (uint32_t)h};
        extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    mSwapchainExtent = extent;

    uint32_t imageCount = std::max(caps.minImageCount + 1, 3u);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = mSurface;
    ci.minImageCount = imageCount;
    ci.imageFormat = mSwapchainFormat;
    ci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(mDevice, &ci, nullptr, &mSwapchain) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create swapchain");
        return;
    }

    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, nullptr);
    mSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

    mSwapchainViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image = mSwapchainImages[i];
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format = mSwapchainFormat;
        viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCI.subresourceRange.levelCount = 1;
        viewCI.subresourceRange.layerCount = 1;
        vkCreateImageView(mDevice, &viewCI, nullptr, &mSwapchainViews[i]);
    }
    createScreenDepth();
}

void VulkanDevice::cleanupSwapchain() {
    if (mScreenColorMSView) { vkDestroyImageView(mDevice, mScreenColorMSView, nullptr); mScreenColorMSView = VK_NULL_HANDLE; }
    if (mScreenColorMSMemory) { vkFreeMemory(mDevice, mScreenColorMSMemory, nullptr); mScreenColorMSMemory = VK_NULL_HANDLE; }
    if (mScreenColorMS) { vkDestroyImage(mDevice, mScreenColorMS, nullptr); mScreenColorMS = VK_NULL_HANDLE; }
    if (mScreenDepthMSView) { vkDestroyImageView(mDevice, mScreenDepthMSView, nullptr); mScreenDepthMSView = VK_NULL_HANDLE; }
    if (mScreenDepthMSMemory) { vkFreeMemory(mDevice, mScreenDepthMSMemory, nullptr); mScreenDepthMSMemory = VK_NULL_HANDLE; }
    if (mScreenDepthMS) { vkDestroyImage(mDevice, mScreenDepthMS, nullptr); mScreenDepthMS = VK_NULL_HANDLE; }
    for (auto v : mSwapchainViews) vkDestroyImageView(mDevice, v, nullptr);
    mSwapchainViews.clear();
    if (mSwapchain) {
        vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
        mSwapchain = VK_NULL_HANDLE;
    }
}

void VulkanDevice::recreateSwapchain() {
    vkDeviceWaitIdle(mDevice);
    cleanupSwapchain();
    createSwapchain();
}

// ──── Command pool ────

void VulkanDevice::createCommandPool() {
    VkCommandPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.queueFamilyIndex = mGraphicsQueueFamily;
    ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    vkCreateCommandPool(mDevice, &ci, nullptr, &mCmdPool);
}

void VulkanDevice::createPerFrameResources(uint32_t frameCount) {
    mFrameCount = frameCount;
    mCmdBuffers.resize(frameCount);
    mFrameFences.resize(frameCount);
    mImageAvailableSemaphores.resize(frameCount);
    mRenderFinishedSemaphores.resize(frameCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mCmdPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = frameCount;
    vkAllocateCommandBuffers(mDevice, &allocInfo, mCmdBuffers.data());
}

void VulkanDevice::createSyncObjects() {
    VkFenceCreateInfo fCI{};
    fCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo sCI{};
    sCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (uint32_t i = 0; i < mFrameCount; ++i) {
        vkCreateFence(mDevice, &fCI, nullptr, &mFrameFences[i]);
        vkCreateSemaphore(mDevice, &sCI, nullptr, &mImageAvailableSemaphores[i]);
        vkCreateSemaphore(mDevice, &sCI, nullptr, &mRenderFinishedSemaphores[i]);
    }
}

// ──── Descriptor pool ────

void VulkanDevice::createDescriptorPool() {
    // Large enough for all shaders × frames
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
    };

    VkDescriptorPoolCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets = 512;
    ci.poolSizeCount = (uint32_t)poolSizes.size();
    ci.pPoolSizes = poolSizes.data();
    ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    vkCreateDescriptorPool(mDevice, &ci, nullptr, &mDescPool);
}

VkDescriptorSet VulkanDevice::allocateDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet ds = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(mDevice, &allocInfo, &ds);
    if (res != VK_SUCCESS) {
        MMD_WARN("VULKAN", "Failed to allocate descriptor set (code=%d)", res);
    }
    return ds;
}

// ──── Uniform ring buffer ────

void VulkanDevice::createUniformRing() {
    mUniformRingSize = 1024 * 1024;  // 1 MB

    ::VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = mUniformRingSize;
    bufCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(mDevice, &bufCI, nullptr, &mUniformRingBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(mDevice, mUniformRingBuffer, &memReqs);

    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    int memIndex = findMemoryType(memReqs.memoryTypeBits, props);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = (uint32_t)memIndex;
    vkAllocateMemory(mDevice, &allocInfo, nullptr, &mUniformRingMemory);
    vkBindBufferMemory(mDevice, mUniformRingBuffer, mUniformRingMemory, 0);
    vkMapMemory(mDevice, mUniformRingMemory, 0, mUniformRingSize, 0, &mUniformRingMapped);
}

VulkanDevice::RingAllocation VulkanDevice::allocateUniformRing(uint32_t size) {
    uint32_t align = mDeviceProps.limits.minUniformBufferOffsetAlignment;
    mUniformRingOffset = (mUniformRingOffset + align - 1) & ~(align - 1);

    if (mUniformRingOffset + size > mUniformRingSize) {
        mUniformRingOffset = 0;
    }

    RingAllocation alloc;
    alloc.buffer = mUniformRingBuffer;
    alloc.offset = mUniformRingOffset;
    alloc.size = size;
    alloc.mappedData = (uint8_t*)mUniformRingMapped + mUniformRingOffset;
    mUniformRingOffset += size;
    return alloc;
}

// ──── Memory ────

int VulkanDevice::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const {
    for (uint32_t i = 0; i < mMemoryProps.memoryTypeCount; ++i) {
        if ((typeBits & (1 << i)) &&
            (mMemoryProps.memoryTypes[i].propertyFlags & props) == props) {
            return (int)i;
        }
    }
    return -1;
}

void VulkanDevice::uploadImage(VkImage image, int w, int h, size_t dataSize, const void* data) {
    // Create staging buffer
    ::VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = dataSize;
    bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    ::VkBuffer stagingBuf;
    vkCreateBuffer(mDevice, &bufCI, nullptr, &stagingBuf);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(mDevice, stagingBuf, &memReqs);
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    int memIndex = findMemoryType(memReqs.memoryTypeBits, props);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = (uint32_t)memIndex;

    VkDeviceMemory stagingMem;
    vkAllocateMemory(mDevice, &allocInfo, nullptr, &stagingMem);
    vkBindBufferMemory(mDevice, stagingBuf, stagingMem, 0);

    void* mapped;
    vkMapMemory(mDevice, stagingMem, 0, dataSize, 0, &mapped);
    memcpy(mapped, data, dataSize);
    vkUnmapMemory(mDevice, stagingMem);

    // Use a temporary command buffer for copy
    VkCommandBufferAllocateInfo cmdAI{};
    cmdAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAI.commandPool = mCmdPool;
    cmdAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAI.commandBufferCount = 1;

    VkCommandBuffer tmpCmd;
    vkAllocateCommandBuffers(mDevice, &cmdAI, &tmpCmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tmpCmd, &beginInfo);

    // Transition image to TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(tmpCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {(uint32_t)w, (uint32_t)h, 1};
    vkCmdCopyBufferToImage(tmpCmd, stagingBuf, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(tmpCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(tmpCmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &tmpCmd;
    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(mGraphicsQueue);

    vkFreeCommandBuffers(mDevice, mCmdPool, 1, &tmpCmd);
    vkFreeMemory(mDevice, stagingMem, nullptr);
    vkDestroyBuffer(mDevice, stagingBuf, nullptr);
}

// ──── Dummy texture ────

void VulkanDevice::createDummyTexture() {
    uint8_t white[] = {255, 255, 255, 255};
    mDummyTexture = std::make_unique<VkTexture>(this, 1, 1, TextureFormat::RGBA8, white);
}

// ──── Shader / Pipeline ────

void VulkanDevice::setCurrentShader(VkShader* shader) {
    mCurrentShader = shader;
    // Allocate per-frame descriptor sets for this shader if needed
    if (mShaderDescSets.find(shader) == mShaderDescSets.end()) {
        ShaderDescriptors sd;
        sd.sets.resize(mFrameCount);
        for (uint32_t i = 0; i < mFrameCount; ++i) {
            sd.sets[i] = allocateDescriptorSet(shader->descriptorSetLayout());
        }
        mShaderDescSets[shader] = sd;
    }
}

// ──── Pipeline cache ────

size_t VulkanDevice::PipelineStateHash::operator()(const PipelineState& s) const {
    size_t h = 0;
    auto hashCombine = [&h](auto v) {
        h ^= std::hash<decltype(v)>()(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
    };
    hashCombine((uintptr_t)s.shader);
    hashCombine((uintptr_t)s.vao);
    hashCombine((int)s.srcBlend);
    hashCombine((int)s.dstBlend);
    hashCombine(s.blendEnable);
    hashCombine(s.depthTest);
    hashCombine((int)s.depthFunc);
    hashCombine((int)s.cullMode);
    hashCombine((int)s.polyMode);
    hashCombine((int)s.primType);
    hashCombine(s.frontFaceClockwise);
    hashCombine(s.hasColorTarget);
    hashCombine(s.sampleCount);
    return h;
}

bool VulkanDevice::PipelineStateEqual::operator()(const PipelineState& a,
                                               const PipelineState& b) const {
    return a.shader == b.shader && a.vao == b.vao &&
           a.srcBlend == b.srcBlend && a.dstBlend == b.dstBlend &&
           a.blendEnable == b.blendEnable && a.depthTest == b.depthTest &&
           a.depthFunc == b.depthFunc && a.cullMode == b.cullMode &&
           a.polyMode == b.polyMode && a.primType == b.primType &&
           a.frontFaceClockwise == b.frontFaceClockwise &&
           a.hasColorTarget == b.hasColorTarget &&
           a.sampleCount == b.sampleCount;
}

VkPipeline VulkanDevice::getOrCreatePipeline(const PipelineState& state) {
    auto it = mPipelineCache.find(state);
    if (it != mPipelineCache.end()) return it->second;

    auto* shader = state.shader;
    auto* vao = state.vao;

    // Shader stages
    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = shader->vsModule();
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = shader->fsModule();
    stages[1].pName = "main";

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = (uint32_t)vao->bindings().size();
    vertexInput.pVertexBindingDescriptions = vao->bindings().data();
    vertexInput.vertexAttributeDescriptionCount = (uint32_t)vao->attributes().size();
    vertexInput.pVertexAttributeDescriptions = vao->attributes().data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = toVkPrimitiveTopology(state.primType);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport (dynamic state)
    VkViewport viewport{};
    viewport.x = (float)mState.viewportX;
    viewport.y = (float)mState.viewportY;
    viewport.width = (float)mState.viewportW;
    viewport.height = (float)mState.viewportH;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {mState.viewportX, mState.viewportY};
    scissor.extent = {(uint32_t)mState.viewportW, (uint32_t)mState.viewportH};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = toVkPolygonMode(state.polyMode);
    rasterizer.cullMode = toVkCullMode(state.cullMode);
    rasterizer.frontFace = state.frontFaceClockwise ? VK_FRONT_FACE_CLOCKWISE
                                                     : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = mState.polygonOffsetEnable;
    rasterizer.depthBiasConstantFactor = mState.polygonOffsetFactor;
    rasterizer.depthBiasSlopeFactor = mState.polygonOffsetUnits;
    rasterizer.lineWidth = mState.lineWidth;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = (VkSampleCountFlagBits)state.sampleCount;

    // Depth/stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = state.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = state.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = toVkCompareOp(state.depthFunc);

    // Color blend
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = state.blendEnable ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = toVkBlendFactor(state.srcBlend);
    colorBlendAttachment.dstColorBlendFactor = toVkBlendFactor(state.dstBlend);
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = toVkBlendFactor(state.srcBlend);
    colorBlendAttachment.dstAlphaBlendFactor = toVkBlendFactor(state.dstBlend);
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic rendering
    VkPipelineRenderingCreateInfo dynRender{};
    dynRender.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    dynRender.colorAttachmentCount = state.hasColorTarget ? 1u : 0u;
    dynRender.pColorAttachmentFormats = state.hasColorTarget ? &mSwapchainFormat : nullptr;
    dynRender.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

    // Dynamic state
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32_t)dynamicStates.size();
    dynamicState.pDynamicStates = dynamicStates.data();

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.pNext = &dynRender;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &rasterizer;
    pipelineCI.pMultisampleState = &multisampling;
    pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.pColorBlendState = &colorBlending;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.layout = shader->pipelineLayout();

    VkPipeline pipeline;
    VkResult res = vkCreateGraphicsPipelines(mDevice, mPipelineCacheVk, 1, &pipelineCI,
                                              nullptr, &pipeline);
    if (res != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create graphics pipeline (code=%d)", res);
        return VK_NULL_HANDLE;
    }

    mPipelineCache[state] = pipeline;
    return pipeline;
}

// ──── Descriptor set update ────

void VulkanDevice::flushDescriptorSet(VkShader* shader) {
    auto it = mShaderDescSets.find(shader);
    if (it == mShaderDescSets.end()) return;

    VkDescriptorSet ds = it->second.sets[mCurrentFrame];
    bool texturesChanged = (shader != mLastFlushedShader || mTextureBindGen != mLastFlushBindGen);
    mLastFlushedShader = shader;
    if (texturesChanged) mLastFlushBindGen = mTextureBindGen;

    // We need to write:
    // - binding 0: UBO (if shader has uniforms)
    // - bindings 1-6: textures
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;
    writes.reserve(7);  // UBO + 6 textures
    bufferInfos.reserve(1);
    imageInfos.reserve(6);

    // UBO
    if (shader->uniformBufferSize() > 0) {
        auto alloc = allocateUniformRing(shader->uniformBufferSize());
        shader->flushUniforms(alloc.mappedData, alloc.size);

        VkDescriptorBufferInfo bufInfo;
        bufInfo.buffer = alloc.buffer;
        bufInfo.offset = alloc.offset;
        bufInfo.range = alloc.size;
        bufferInfos.push_back(bufInfo);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = ds;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos.back();
        writes.push_back(write);
    }

    // Texture bindings — skip when unchanged (same shader, no texture rebinds)
    if (texturesChanged) {
    for (int i = 0; i < 6; ++i) {
        VkTexture* tex = mBoundTextures[i];
        if (!tex) tex = mDummyTexture.get();

        imageInfos.push_back({});
        VkDescriptorImageInfo& imgInfo = imageInfos.back();
        imgInfo.sampler = tex->sampler();
        imgInfo.imageView = tex->imageView();
        // Use the texture's actual current layout (depth textures use DEPTH_READ_ONLY_OPTIMAL)
        VkImageLayout texLayout = tex->currentLayout();
        if (texLayout == VK_IMAGE_LAYOUT_UNDEFINED || texLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            || texLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
            texLayout = tex->isDepth() ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
                                       : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageLayout = texLayout;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = ds;
        write.dstBinding = (uint32_t)(i + 1);
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfos.back();
        writes.push_back(write);
    }
    }  // texturesChanged

    vkUpdateDescriptorSets(mDevice, (uint32_t)writes.size(), writes.data(), 0, nullptr);

    // Bind descriptor set
    vkCmdBindDescriptorSets(mCurrentCmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                           shader->pipelineLayout(), 0, 1, &ds, 0, nullptr);

    mCurrentDescSet = ds;
}

// ──── Draw ────

void VulkanDevice::drawVertexArray(VkVertexArray* vao, PrimitiveType prim, int count, int first) {
    if (!mCurrentShader || !mCmdRecording) return;

    // Flush uniform + texture descriptor set
    flushDescriptorSet(mCurrentShader);

    // Build pipeline state key
    PipelineState psoState;
    psoState.shader = mCurrentShader;
    psoState.vao = vao;
    psoState.srcBlend = mState.srcBlend;
    psoState.dstBlend = mState.dstBlend;
    psoState.blendEnable = mState.blendEnable;
    psoState.depthTest = mState.depthTest;
    psoState.depthFunc = mState.depthFunc;
    psoState.cullMode = mState.cullMode;
    psoState.polyMode = mState.polyMode;
    psoState.primType = prim;
    psoState.frontFaceClockwise = mState.frontFaceClockwise;
    psoState.hasColorTarget = mCurrentRT ? mCurrentRT->hasColor() : true;
    psoState.sampleCount = psoState.hasColorTarget ? (uint32_t)mMsaaSamples : 1;

    // Get or create pipeline
    VkPipeline pipeline = getOrCreatePipeline(psoState);
    if (!pipeline) return;

    vkCmdBindPipeline(mCurrentCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Set viewport/scissor (dynamic state)
    // Vulkan NDC has Y-down; flip to match OpenGL convention (Y-up)
    VkViewport vp{};
    vp.x = (float)mState.viewportX;
    vp.y = (float)mState.viewportH;
    vp.width = (float)mState.viewportW;
    vp.height = -(float)mState.viewportH;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(mCurrentCmd, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {mState.viewportX, mState.viewportY};
    scissor.extent = {(uint32_t)mState.viewportW, (uint32_t)mState.viewportH};
    vkCmdSetScissor(mCurrentCmd, 0, 1, &scissor);

    // Bind vertex/index buffers
    std::vector<VkDeviceSize> offsets(vao->vkBuffers().size(), 0);
    vkCmdBindVertexBuffers(mCurrentCmd, 0, (uint32_t)vao->vkBuffers().size(),
                          vao->vkBuffers().data(), offsets.data());

    if (vao->hasIndexBuffer()) {
        vkCmdBindIndexBuffer(mCurrentCmd, vao->vkIndexBuffer(), 0,
                            toVkIndexType(vao->indexType()));

        int idxCount = count >= 0 ? count : vao->indexCount();
        int firstIdx = first;
        vkCmdDrawIndexed(mCurrentCmd, idxCount, 1, firstIdx, 0, 0);
    } else {
        int vCount = count >= 0 ? count : vao->vertexCount();
        vkCmdDraw(mCurrentCmd, vCount, 1, first, 0);
    }
}

// ──── State setters ────

void VulkanDevice::setViewport(int x, int y, int w, int h) {
    mState.viewportX = x;
    mState.viewportY = y;
    // Only mark swapchain dirty if the screen size actually changed.
    // (bindRenderTarget/bindScreenFramebuffer also write viewportW/H,
    //  so check against the last known screen size, not current state.)
    if (mScreenW != w || mScreenH != h) {
        mSwapchainDirty = true;
        mScreenW = w;
        mScreenH = h;
    }
    mState.viewportW = w;
    mState.viewportH = h;
}

void VulkanDevice::setClearColor(float r, float g, float b, float a) {
    mState.clearR = r;
    mState.clearG = g;
    mState.clearB = b;
    mState.clearA = a;
}

void VulkanDevice::clear(bool color, bool depth) {
    if (!mCmdRecording) return;

    // End current dynamic rendering if active, then begin a new one with clear
    // For simplicity, we cancel the current rendering and start fresh
    // Since we use LOAD_OP_CLEAR at the start of dynamic rendering,
    // we handle clear in bindRenderTarget / bindScreenFramebuffer.

    // If no current render pass is active, we can't clear — the clear
    // is handled when the render pass / dynamic rendering begins.
    // For mid-pass clears (which shouldn't happen in this codebase), we'd need
    // vkCmdClearAttachments.
    (void)color; (void)depth;
}

void VulkanDevice::setDepthTest(bool enable) {
    mState.depthTest = enable;
}

void VulkanDevice::setDepthFunc(CompareFunc func) {
    mState.depthFunc = func;
}

void VulkanDevice::setBlend(bool enable) {
    mState.blendEnable = enable;
}

void VulkanDevice::setBlendFunc(BlendFactor src, BlendFactor dst) {
    mState.srcBlend = src;
    mState.dstBlend = dst;
}

void VulkanDevice::setCullMode(CullMode mode) {
    mState.cullMode = mode;
}

void VulkanDevice::setFrontFace(bool clockwise) {
    mState.frontFaceClockwise = clockwise;
}

void VulkanDevice::setPolygonMode(PolygonMode mode) {
    mState.polyMode = mode;
}

void VulkanDevice::setPolygonOffset(float factor, float units) {
    mState.polygonOffsetEnable = (factor != 0.0f || units != 0.0f);
    mState.polygonOffsetFactor = factor;
    mState.polygonOffsetUnits = units;
}

void VulkanDevice::setLineWidth(float width) {
    mState.lineWidth = width;
}

// ──── Texture binding ────

void VulkanDevice::bindTextureToUnit(int unit, IGpuTexture* tex) {
    if (unit >= 0 && unit < 6) {
        mBoundTextures[unit] = static_cast<VkTexture*>(tex);
        mTextureBindGen++;
    }
}

// ──── Framebuffer / Render Target ────

void VulkanDevice::bindRenderTarget(VkRenderTarget* rt) {
    // End previous rendering and transition attachments to shader-readable
    if (mCmdRecording && mRenderingActive) {
        vkCmdEndRendering(mCurrentCmd);
        // Transition depth attachment so it can be sampled (e.g. shadow map)
        if (mCurrentRT && mCurrentRT->hasDepth() && mCurrentRT->depthTexture()) {
            auto* depTex = static_cast<VkTexture*>(mCurrentRT->depthTexture());
            bool wasDepth = (depTex->currentLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
            if (wasDepth) {
                VkImageMemoryBarrier b{};
                b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                b.newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image = depTex->image();
                b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                b.subresourceRange.levelCount = 1;
                b.subresourceRange.layerCount = 1;
                b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(mCurrentCmd,
                                     VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &b);
                depTex->setLayout(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
            }
        }
        mRenderingActive = false;
    }

    mCurrentRT = rt;

    // Update viewport to match render target size
    mState.viewportX = 0;
    mState.viewportY = 0;
    mState.viewportW = rt->width();
    mState.viewportH = rt->height();

    // Transition images from shader-read to attachment layout
    std::vector<VkImageMemoryBarrier> barriers;

    if (rt->hasColor() && rt->colorTexture()) {
        auto* colTex = static_cast<VkTexture*>(rt->colorTexture());
        if (colTex->currentLayout() != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout = colTex->currentLayout();
            b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = colTex->image();
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.layerCount = 1;
            b.srcAccessMask = 0;
            b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barriers.push_back(b);
            colTex->setLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }

    if (rt->hasDepth() && rt->depthTexture()) {
        auto* depTex = static_cast<VkTexture*>(rt->depthTexture());
        if (depTex->currentLayout() != VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout = depTex->currentLayout();
            b.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = depTex->image();
            b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            b.subresourceRange.levelCount = 1;
            b.subresourceRange.layerCount = 1;
            b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            b.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            barriers.push_back(b);
            depTex->setLayout(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        }
    }

    if (!barriers.empty()) {
        vkCmdPipelineBarrier(mCurrentCmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0,
                             0, nullptr, 0, nullptr,
                             (uint32_t)barriers.size(), barriers.data());
    }

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = rt->colorView();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {
        mState.clearR, mState.clearG, mState.clearB, mState.clearA};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = rt->depthView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {(uint32_t)rt->width(), (uint32_t)rt->height()}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = rt->hasColor() ? 1u : 0u;
    renderingInfo.pColorAttachments = rt->hasColor() ? &colorAttachment : nullptr;
    renderingInfo.pDepthAttachment = rt->hasDepth() ? &depthAttachment : nullptr;

    vkCmdBeginRendering(mCurrentCmd, &renderingInfo);
    mRenderingActive = true;
}

void VulkanDevice::bindScreenFramebuffer(int w, int h) {
    mState.viewportW = w;
    mState.viewportH = h;

    if (mCmdRecording && mRenderingActive) {
        vkCmdEndRendering(mCurrentCmd);
        // Transition depth attachment for reading (shadow map → main pass)
        if (mCurrentRT && mCurrentRT->hasDepth() && mCurrentRT->depthTexture()) {
            auto* depTex = static_cast<VkTexture*>(mCurrentRT->depthTexture());
            if (depTex->currentLayout() == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) {
                VkImageMemoryBarrier b{};
                b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                b.newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image = depTex->image();
                b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                b.subresourceRange.levelCount = 1;
                b.subresourceRange.layerCount = 1;
                b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(mCurrentCmd,
                                     VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                                     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                     0, 0, nullptr, 0, nullptr, 1, &b);
                depTex->setLayout(VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
            }
        }
        mRenderingActive = false;
    }

    mCurrentRT = nullptr;

    // Transition swapchain image from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
    if (mCurrentImageIndex < mSwapchainImages.size()) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // first frame or after resize
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = mSwapchainImages[mCurrentImageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vkCmdPipelineBarrier(mCurrentCmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = mScreenColorMSView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = mSwapchainViews[mCurrentImageIndex];
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {
        mState.clearR, mState.clearG, mState.clearB, mState.clearA};

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = mScreenDepthMSView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {{0, 0}, {(uint32_t)w, (uint32_t)h}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(mCurrentCmd, &renderingInfo);
    mRenderingActive = true;
}

// ──── Frame management ────

void VulkanDevice::beginFrame() {
    if (mSwapchainDirty) {
        vkDeviceWaitIdle(mDevice);
        recreateSwapchain();
        mSwapchainDirty = false;
    }

    mCurrentFrame = (mCurrentFrame + 1) % mFrameCount;

    // Wait for previous frame
    vkWaitForFences(mDevice, 1, &mFrameFences[mCurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(mDevice, 1, &mFrameFences[mCurrentFrame]);

    // Acquire swapchain image (retry after resize)
    for (int retry = 0; retry < 2; ++retry) {
        VkResult result = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX,
                                                 mImageAvailableSemaphores[mCurrentFrame],
                                                 VK_NULL_HANDLE, &mCurrentImageIndex);
        if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) break;
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            continue;
        }
        return;  // fatal error, skip frame
    }

    // Begin command buffer
    mCurrentCmd = mCmdBuffers[mCurrentFrame];
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(mCurrentCmd, &beginInfo);
    mCmdRecording = true;

    // Reset uniform ring for this frame
    mUniformRingOffset = 0;

    // Reset state
    for (int i = 0; i < 6; ++i) mBoundTextures[i] = nullptr;

    mCurrentShader = nullptr;
    mCurrentRT = nullptr;
    mRenderingActive = false;
}

void VulkanDevice::endFrame() {
    if (!mCmdRecording) return;

    // End any active rendering
    if (mRenderingActive) {
        vkCmdEndRendering(mCurrentCmd);
        mRenderingActive = false;
    }

    // Transition swapchain image to present layout
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = mSwapchainImages[mCurrentImageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(mCurrentCmd,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(mCurrentCmd);

    // Submit
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &mImageAvailableSemaphores[mCurrentFrame];
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCurrentCmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mRenderFinishedSemaphores[mCurrentFrame];

    vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mFrameFences[mCurrentFrame]);

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mRenderFinishedSemaphores[mCurrentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.pImageIndices = &mCurrentImageIndex;

    VkResult result = vkQueuePresentKHR(mGraphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }

    mCmdRecording = false;
    mCurrentCmd = VK_NULL_HANDLE;
}

// ──── Resource creation ────

std::unique_ptr<IGpuBuffer> VulkanDevice::createVertexBuffer(const void* data, size_t bytes,
                                                           BufferUsage usage) {
    VkBufferUsageFlags vkUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bool cpuWritable = (usage == BufferUsage::Dynamic);
    return std::make_unique<VkBuffer>(this, data, bytes, vkUsage, cpuWritable);
}

std::unique_ptr<IGpuBuffer> VulkanDevice::createIndexBuffer(const void* data, size_t bytes,
                                                          IndexType type) {
    (void)type;
    VkBufferUsageFlags vkUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    return std::make_unique<VkBuffer>(this, data, bytes, vkUsage, false);
}

std::unique_ptr<IGpuTexture> VulkanDevice::createTexture(int w, int h, TextureFormat fmt,
                                                       const void* data) {
    return std::make_unique<VkTexture>(this, w, h, fmt, data);
}

std::unique_ptr<IGpuShader> VulkanDevice::createShader(const std::string& vertexSrc,
                                                     const std::string& fragmentSrc) {
    return std::make_unique<VkShader>(this, vertexSrc, fragmentSrc);
}

std::unique_ptr<IGpuVertexArray>
VulkanDevice::createVertexArray(const std::vector<VertexAttribute>& attributes,
                             const std::vector<IGpuBuffer*>& vertexBuffers,
                             IGpuBuffer* indexBuffer, IndexType indexType,
                             int vertexCount, int indexCount) {
    return std::make_unique<VkVertexArray>(this, attributes, vertexBuffers,
                                            indexBuffer, indexType,
                                            vertexCount, indexCount);
}

std::unique_ptr<IGpuRenderTarget> VulkanDevice::createRenderTarget(int w, int h,
                                                                 bool withColor,
                                                                 bool withDepth) {
    auto rt = std::make_unique<VkRenderTarget>();
    rt->init(this, w, h, withColor, withDepth);
    // Enable depth comparison for shadow map (depth-only RT)
    if (!withColor && withDepth && rt->depthTexture()) {
        static_cast<VkTexture*>(rt->depthTexture())->enableCompare();
    }
    return rt;
}

}  // namespace Gpu
