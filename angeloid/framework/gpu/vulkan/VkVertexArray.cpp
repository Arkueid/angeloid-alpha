#include "framework/gpu/vulkan/VkVertexArray.h"
#include "framework/gpu/vulkan/VkBuffer.h"
#include "framework/gpu/vulkan/VkDevice.h"
#include "framework/gpu/vulkan/VkShader.h"
#include "framework/gpu/vulkan/VkTypes.h"

namespace Gpu {

VkVertexArray::VkVertexArray(VulkanDevice* device,
                               const std::vector<VertexAttribute>& attributes,
                               const std::vector<IGpuBuffer*>& vertexBuffers,
                               IGpuBuffer* indexBuffer,
                               IndexType indexType,
                               int vertexCount,
                               int indexCount)
    : mDevice(device), mVertexCount(vertexCount), mIndexCount(indexCount),
      mIndexType(indexType), mHasIndexBuffer(indexBuffer != nullptr) {

    if (indexBuffer) {
        auto* vkIdxBuf = static_cast<VkBuffer*>(indexBuffer);
        mVkIndexBuffer = vkIdxBuf->buffer();
    }

    // Build vertex input state — assign binding numbers by unique buffer pointer
    std::unordered_map<IGpuBuffer*, int> bufToBinding;

    for (size_t i = 0; i < attributes.size(); ++i) {
        const auto& attr = attributes[i];
        IGpuBuffer* buf = vertexBuffers[i];

        int bind;
        auto it = bufToBinding.find(buf);
        if (it != bufToBinding.end()) {
            bind = it->second;
        } else {
            bind = (int)bufToBinding.size();
            bufToBinding[buf] = bind;

            VkVertexInputBindingDescription bindDesc{};
            bindDesc.binding = (uint32_t)bind;
            bindDesc.stride = (uint32_t)(attr.stride > 0 ? attr.stride : 0);
            bindDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            mBindings.push_back(bindDesc);
            // Store VkBuffer handle indexed by binding number
            mVkBuffers.push_back(static_cast<VkBuffer*>(buf)->buffer());
        }

        VkVertexInputAttributeDescription attrDesc{};
        attrDesc.location = (uint32_t)attr.location;
        attrDesc.binding = (uint32_t)bind;
        attrDesc.format = toVkAttributeFormat(attr.dtype, attr.size);
        attrDesc.offset = (uint32_t)attr.offset;
        mAttribs.push_back(attrDesc);
    }

    // If strides are 0 (tightly packed), set actual stride from attribute sizes
    for (auto& b : mBindings) {
        if (b.stride == 0) {
            uint32_t maxStride = 0;
            for (auto& a : mAttribs) {
                if (a.binding == b.binding) {
                    uint32_t attrSize = 0;
                    switch (a.format) {
                    case VK_FORMAT_R32_SFLOAT:          attrSize = 4;  break;
                    case VK_FORMAT_R32G32_SFLOAT:       attrSize = 8;  break;
                    case VK_FORMAT_R32G32B32_SFLOAT:    attrSize = 12; break;
                    case VK_FORMAT_R32G32B32A32_SFLOAT: attrSize = 16; break;
                    case VK_FORMAT_R32_SINT:            attrSize = 4;  break;
                    case VK_FORMAT_R32G32_SINT:         attrSize = 8;  break;
                    case VK_FORMAT_R32G32B32_SINT:      attrSize = 12; break;
                    case VK_FORMAT_R32G32B32A32_SINT:   attrSize = 16; break;
                    case VK_FORMAT_R32_UINT:            attrSize = 4;  break;
                    default:                            attrSize = 4;  break;
                    }
                    maxStride = std::max(maxStride, a.offset + attrSize);
                }
            }
            b.stride = maxStride;
        }
    }
}

VkVertexArray::~VkVertexArray() = default;

void VkVertexArray::draw(PrimitiveType prim, int count, int first) {
    mDevice->drawVertexArray(this, prim, count, first);
}

}  // namespace Gpu
