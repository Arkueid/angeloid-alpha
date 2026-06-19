#pragma once

#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/Types.h"

#include <vulkan/vulkan.h>
#include <array>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <deque>

struct GLFWwindow;

namespace Gpu {

class VkBuffer;
class VkTexture;
class VkShader;
class VkVertexArray;
class VkRenderTarget;
struct VertexAttribute;

// ── VulkanDevice — Vulkan backend implementing IGpuDevice ──
//
// Emulates OpenGL state machine: setDepthTest() etc. modify internal flags
// that become part of the pipeline cache key. Pipelines are created lazily
// when VkVertexArray::draw() is called.

class VulkanDevice final : public IGpuDevice {
public:
    VulkanDevice(GLFWwindow* window);
    ~VulkanDevice() override;

    // ── IGpuDevice interface ──

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

    void bindScreenFramebuffer(int w, int h) override;
    void bindTextureToUnit(int unit, IGpuTexture* tex) override;

    bool needsDepthCorrection() const override { return true; }
    VulkanDevice* asVulkan() override { return this; }

    // ── Frame management (IGpuDevice overrides) ──
    void beginFrame() override;
    void endFrame() override;

    // ── ImGui integration: expose Vulkan handles ──
    VkInstance vkInstance() const { return mInstance; }
    VkSampleCountFlagBits msaaSamples() const { return mMsaaSamples; }

    // ── Internal: called by Vk* classes ──

    ::VkDevice device() const { return mDevice; }
    VkPhysicalDevice physicalDevice() const { return mPhysicalDevice; }
    VkCommandBuffer currentCmd() const { return mCurrentCmd; }
    uint32_t graphicsQueueFamily() const { return mGraphicsQueueFamily; }
    VkQueue graphicsQueue() const { return mGraphicsQueue; }
    VkRenderPass currentRenderPass() const { return VK_NULL_HANDLE; }  // dynamic rendering

    int findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
    void uploadImage(VkImage image, int w, int h, size_t dataSize, const void* data);

    void setCurrentShader(VkShader* shader);
    void drawVertexArray(VkVertexArray* vao, PrimitiveType prim, int count, int first);
    void bindRenderTarget(VkRenderTarget* rt);

    // Descriptor set allocation for per-shader texture bindings
    VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);

    // The dummy 1x1 white texture for unbound texture slots
    VkTexture* dummyTexture() const { return mDummyTexture.get(); }

    // Uniform buffer ring allocation
    struct RingAllocation {
        ::VkBuffer buffer;
        uint32_t offset;
        uint32_t size;
        void* mappedData;
    };
    RingAllocation allocateUniformRing(uint32_t size);

private:
    // ── Initialization ──
    void createInstance();
    void createSurface();
    void selectPhysicalDevice();
    void createLogicalDevice();
    void createMsImage(uint32_t w, uint32_t h, VkFormat fmt,
                       VkImageUsageFlags usage, VkImageAspectFlags aspect,
                       VkImageLayout targetLayout,
                       VkImage& img, VkDeviceMemory& mem, VkImageView& view);
    void createScreenDepth();
    void createSwapchain();
    void createCommandPool();
    void createDescriptorPool();
    void createPerFrameResources(uint32_t frameCount);
    void createSyncObjects();
    void createUniformRing();
    void createDummyTexture();

    // Pipeline cache
    struct PipelineState {
        VkShader* shader;
        VkVertexArray* vao;
        BlendFactor srcBlend, dstBlend;
        bool blendEnable;
        bool depthTest;
        CompareFunc depthFunc;
        CullMode cullMode;
        PolygonMode polyMode;
        PrimitiveType primType;
        bool frontFaceClockwise;
        bool hasColorTarget;
        uint32_t sampleCount;
    };
    struct PipelineStateHash {
        size_t operator()(const PipelineState& s) const;
    };
    struct PipelineStateEqual {
        bool operator()(const PipelineState& a, const PipelineState& b) const;
    };
    VkPipeline getOrCreatePipeline(const PipelineState& state);

    // Descriptor set update
    void flushDescriptorSet(VkShader* shader);

    // Swapchain
    void recreateSwapchain();
    void cleanupSwapchain();

    // GLSL→SPIR-V (delegates to VkShader static method via glslc)

    // Window
    GLFWwindow* mWindow = nullptr;

    // Vulkan handles
    VkInstance mInstance = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    ::VkDevice mDevice = VK_NULL_HANDLE;
    uint32_t mGraphicsQueueFamily = 0;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    VkFormat mSwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D mSwapchainExtent{};
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainViews;
    uint32_t mCurrentImageIndex = 0;

    // MSAA screen resources (not provided by swapchain)
    VkSampleCountFlagBits mMsaaSamples = VK_SAMPLE_COUNT_4_BIT;
    VkImage mScreenColorMS = VK_NULL_HANDLE;
    VkDeviceMemory mScreenColorMSMemory = VK_NULL_HANDLE;
    VkImageView mScreenColorMSView = VK_NULL_HANDLE;
    VkImage mScreenDepthMS = VK_NULL_HANDLE;
    VkDeviceMemory mScreenDepthMSMemory = VK_NULL_HANDLE;
    VkImageView mScreenDepthMSView = VK_NULL_HANDLE;

    // Current frame resources
    VkCommandPool mCmdPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCmdBuffers;
    std::vector<VkFence> mFrameFences;
    std::vector<VkSemaphore> mImageAvailableSemaphores;
    std::vector<VkSemaphore> mRenderFinishedSemaphores;
    uint32_t mCurrentFrame = 0;
    uint32_t mFrameCount = 0;

    VkCommandBuffer mCurrentCmd = VK_NULL_HANDLE;
    bool mCmdRecording = false;

    // Descriptor pool
    VkDescriptorPool mDescPool = VK_NULL_HANDLE;

    // Dummy resources
    std::unique_ptr<VkTexture> mDummyTexture;

    // Emulated GL state
    struct EmulatedState {
        int viewportX = 0, viewportY = 0, viewportW = 0, viewportH = 0;
        float clearR = 0, clearG = 0, clearB = 0, clearA = 1;
        bool depthTest = true;
        CompareFunc depthFunc = CompareFunc::LEqual;
        bool blendEnable = true;
        BlendFactor srcBlend = BlendFactor::SrcAlpha;
        BlendFactor dstBlend = BlendFactor::OneMinusSrcAlpha;
        CullMode cullMode = CullMode::None;
        bool frontFaceClockwise = true;
        PolygonMode polyMode = PolygonMode::Fill;
        float lineWidth = 1.0f;
        bool polygonOffsetEnable = false;
        float polygonOffsetFactor = 0;
        float polygonOffsetUnits = 0;
    } mState;

    // Current shader
    VkShader* mCurrentShader = nullptr;

    // Current render target (nullptr = screen/swapchain)
    VkRenderTarget* mCurrentRT = nullptr;
    bool mRenderingActive = false;
    bool mSwapchainDirty = false;
    int mScreenW = 0, mScreenH = 0;  // last screen size from setViewport (not bindRenderTarget)

    // Pipeline cache
    std::unordered_map<PipelineState, VkPipeline, PipelineStateHash, PipelineStateEqual>
        mPipelineCache;
    VkPipelineCache mPipelineCacheVk = VK_NULL_HANDLE;

    // Per-shader descriptor sets (one per frame for texture bindings + UBO)
    struct ShaderDescriptors {
        std::vector<VkDescriptorSet> sets;  // one per frame
    };
    std::unordered_map<VkShader*, ShaderDescriptors> mShaderDescSets;

    // Texture bindings (staged until next draw)
    VkTexture* mBoundTextures[6] = {};  // unit 0..5
    VkDescriptorSet mCurrentDescSet = VK_NULL_HANDLE;
    VkShader* mLastFlushedShader = nullptr;
    uint64_t mTextureBindGen = 0;
    uint64_t mLastFlushBindGen = 0;

    // Uniform ring buffer
    ::VkBuffer mUniformRingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mUniformRingMemory = VK_NULL_HANDLE;
    void* mUniformRingMapped = nullptr;
    uint32_t mUniformRingSize = 0;      // 1 MB
    uint32_t mUniformRingOffset = 0;    // per-frame reset

    // Physical device properties
    VkPhysicalDeviceProperties mDeviceProps{};
    VkPhysicalDeviceMemoryProperties mMemoryProps{};

    friend class VkBuffer;
    friend class VkTexture;
    friend class VkShader;
    friend class VkVertexArray;
    friend class VkRenderTarget;
};

}  // namespace Gpu
