#pragma once

#include "framework/gpu/IGpuShader.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Gpu {

class VulkanDevice;

struct UniformInfo {
    uint32_t offset = 0;
    uint32_t size = 0;
};

class VkShader final : public IGpuShader {
public:
    VkShader(VulkanDevice* device, const std::string& vertexSrc,
             const std::string& fragmentSrc);
    ~VkShader() override;

    VkShader(const VkShader&) = delete;
    VkShader& operator=(const VkShader&) = delete;

    void use() override;
    void setInt(const std::string& name, int value) override;
    void setFloat(const std::string& name, float value) override;
    void setVec2(const std::string& name, float x, float y) override;
    void setVec3(const std::string& name, float x, float y, float z) override;
    void setVec4(const std::string& name, float x, float y, float z, float w) override;
    void setMat4(const std::string& name, const float* data) override;

    // Used by VulkanDevice for pipeline creation and draw
    VkShaderModule vsModule() const { return mVsModule; }
    VkShaderModule fsModule() const { return mFsModule; }
    VkPipelineLayout pipelineLayout() const { return mPipelineLayout; }
    VkDescriptorSetLayout descriptorSetLayout() const { return mDescSetLayout; }
    uint32_t uniformBufferSize() const { return mUniformSize; }

    // Write accumulated uniforms to a buffer and return number of bytes written
    uint32_t flushUniforms(void* dst, uint32_t maxSize) const;

private:
    static std::vector<uint32_t> compileGLSL(const std::string& src,
                                              VkShaderStageFlagBits stage);
    void destroy();

    VulkanDevice* mDevice;
    VkShaderModule mVsModule = VK_NULL_HANDLE;
    VkShaderModule mFsModule = VK_NULL_HANDLE;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDescSetLayout = VK_NULL_HANDLE;

    uint32_t mUniformSize = 0;
    std::vector<uint8_t> mUniformData;
    std::unordered_map<std::string, UniformInfo> mUniformLayout;
};

}  // namespace Gpu
