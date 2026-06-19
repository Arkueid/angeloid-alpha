#include "framework/gpu/vulkan/VkShader.h"
#include "framework/gpu/vulkan/VkDevice.h"

#include "core/util/Log.h"

#include <shaderc/shaderc.h>

#include <algorithm>
#include <chrono>
#include <array>
#include <cstring>
#include <string>

namespace Gpu {

// ──── SPIR-V minimal reflection ────

// Walk SPIR-V binary to find uniform block member offsets.
// Looks for OpMemberDecorate instructions on the uniform block struct.
// Returns map of member_name → UniformInfo and total block size.
static void parseSpirvUniformBlock(const uint32_t* code, size_t wordCount,
                                   std::unordered_map<std::string, UniformInfo>& outLayout,
                                   uint32_t& outBlockSize) {
    // SPIR-V constants (from spirv.h / SPIR-V spec)
    constexpr uint32_t OpDecorate = 71;
    constexpr uint32_t OpMemberDecorate = 72;
    constexpr uint32_t OpTypeVector = 23;
    constexpr uint32_t OpTypeMatrix = 24;
    constexpr uint32_t OpTypeFloat = 22;
    constexpr uint32_t OpTypeInt = 21;
    constexpr uint32_t OpTypeStruct = 30;
    constexpr uint32_t OpTypePointer = 32;
    constexpr uint32_t OpVariable = 59;
    constexpr uint32_t OpName = 5;
    constexpr uint32_t OpMemberName = 6;
    constexpr uint32_t DecorationBlock = 2;
    constexpr uint32_t DecorationOffset = 35;
    constexpr uint32_t StorageClassUniform = 2;

    // Find the uniform block struct via OpVariable with StorageClass Uniform.
    uint32_t uniformPtrType = 0;
    for (size_t i = 5; i < wordCount; ) {
        uint32_t wordCount = code[i] >> 16;
        uint32_t opcode = code[i] & 0xFFFF;
        if (opcode == OpVariable && wordCount >= 4 &&
            code[i + 3] == StorageClassUniform) {
            uniformPtrType = code[i + 1];
            break;
        }
        i += wordCount;
    }

    // shaderc may use UniformConstant storage class for Vulkan uniform blocks
    if (uniformPtrType == 0) {
        for (size_t i = 5; i < wordCount; ) {
            uint32_t wordCount = code[i] >> 16;
            uint32_t opcode = code[i] & 0xFFFF;
            // UniformConstant = 0, but also check for any OpVariable with a Block-decorated type
            if (opcode == OpVariable && wordCount >= 4) {
                uint32_t sc = code[i + 3];
                if (sc == 0) {  // UniformConstant
                    uniformPtrType = code[i + 1];
                    break;
                }
            }
            i += wordCount;
        }
    }

    if (uniformPtrType == 0) {
        // Fallback: find any Block-decorated struct (exclude gl_PerVertex by name)
        for (size_t i = 5; i < wordCount; ) {
            uint32_t wordCount = code[i] >> 16;
            uint32_t opcode = code[i] & 0xFFFF;
            if (opcode == OpDecorate && wordCount >= 3 &&
                code[i + 2] == DecorationBlock) {
                uint32_t candidate = code[i + 1];
                // Check if this struct has a name different from gl_PerVertex
                bool isPerVertex = false;
                for (size_t j = 5; j < wordCount; ) {
                    uint32_t wc2 = code[j] >> 16;
                    uint32_t op2 = code[j] & 0xFFFF;
                    if (op2 == OpName && wc2 >= 3 && code[j + 1] == candidate) {
                        const char* nm = (const char*)&code[j + 2];
                        if (strstr(nm, "gl_PerVertex")) { isPerVertex = true; break; }
                    }
                    j += wc2;
                }
                if (!isPerVertex) {
                    uniformPtrType = 0;  // Need pointer type; walk OpTypePointer
                    for (size_t j = 5; j < wordCount; ) {
                        uint32_t wc3 = code[j] >> 16;
                        uint32_t op3 = code[j] & 0xFFFF;
                        if (op3 == OpTypePointer && wc3 >= 4 && code[j + 3] == candidate) {
                            uniformPtrType = code[j + 1];
                            break;
                        }
                        j += wc3;
                    }
                    if (uniformPtrType != 0) break;
                }
            }
            i += wordCount;
        }
    }

    if (uniformPtrType == 0) {
        outBlockSize = 0;
        return;
    }

    // Follow the pointer type to get the struct type
    uint32_t blockStructId = 0;
    for (size_t i = 5; i < wordCount; ) {
        uint32_t wordCount = code[i] >> 16;
        uint32_t opcode = code[i] & 0xFFFF;
        if (opcode == OpTypePointer && wordCount >= 4 &&
            code[i + 1] == uniformPtrType) {
            blockStructId = code[i + 3];  // the pointee type (struct)
            break;
        }
        i += wordCount;
    }

    if (blockStructId == 0) {
        outBlockSize = 0;
        return;
    }

    // Second pass: collect member names (OpMemberName) and offsets (OpMemberDecorate)
    std::unordered_map<uint32_t, std::string> memberNames; // memberIndex → name
    std::unordered_map<uint32_t, uint32_t> memberOffsets;   // memberIndex → offset

    for (size_t i = 5; i < wordCount; ) {
        uint32_t wc = code[i] >> 16;
        uint32_t op = code[i] & 0xFFFF;
        if (op == OpMemberName && wc >= 4 && code[i + 1] == blockStructId) {
            uint32_t memberIdx = code[i + 2];
            const char* name = (const char*)&code[i + 3];
            memberNames[memberIdx] = name;
        }
        if (op == OpMemberDecorate && wc >= 5 && code[i + 1] == blockStructId &&
            code[i + 3] == DecorationOffset) {
            uint32_t memberIdx = code[i + 2];
            memberOffsets[memberIdx] = code[i + 4];
        }
        i += wc;
    }

    // Third pass: get member types from the struct definition
    // Find OpTypeStruct %blockStructId %type0 %type1 ...
    // Also look for OpTypeMatrix which needs MatrixStride decoration
    std::vector<uint32_t> memberTypeIds;
    for (size_t i = 5; i < wordCount; ) {
        uint32_t wc = code[i] >> 16;
        uint32_t op = code[i] & 0xFFFF;
        if (op == OpTypeStruct && wc >= 2 && code[i + 1] == blockStructId) {
            uint32_t memberCount = wc - 2;
            for (uint32_t m = 0; m < memberCount; ++m) {
                memberTypeIds.push_back(code[i + 2 + m]);
            }
            break;
        }
        i += wc;
    }

    // Determine size of each member type (approximate: use vec4 alignment rules)
    // TODO: proper std140 layout calculation
    // For now use these rules:
    // - scalar: 4 bytes, aligned to 4
    // - vec2: 8 bytes, aligned to 8
    // - vec3: 12 bytes, aligned to 16
    // - vec4: 16 bytes, aligned to 16
    // - mat4: 64 bytes, aligned to 16 (column-major, each column is vec4)
    // - mat3: 48 bytes, aligned to 16 (column-major, each column is vec4 in std140)

    auto typeSize = [&](uint32_t typeId) -> uint32_t {
        for (size_t i = 5; i < wordCount; ) {
            uint32_t wc2 = code[i] >> 16;
            uint32_t op2 = code[i] & 0xFFFF;
            if (op2 == OpTypeVector && wc2 >= 4 && code[i + 1] == typeId) {
                uint32_t comps = code[i + 3];
                if (comps == 3) return 16;  // vec3 padded to vec4 in std140
                return comps * 4;
            }
            if (op2 == 24 && wc2 >= 3 && code[i + 1] == typeId) {  // OpTypeMatrix
                // All MMD shaders use mat4 (64 bytes). mat3 would be 48.
                return 64;
            }
            if (op2 == 22 && wc2 >= 2 && code[i + 1] == typeId) return 4;  // float
            if (op2 == 21 && wc2 >= 2 && code[i + 1] == typeId) return 4;  // int
            if (op2 == 20 && wc2 >= 2 && code[i + 1] == typeId) return 4;  // bool
            i += wc2;
        }
        return 4;  // default
    };

    // Build the uniform layout from member names, offsets, and sizes
    outBlockSize = 0;
    for (size_t m = 0; m < memberTypeIds.size(); ++m) {
        std::string name;
        auto itName = memberNames.find((uint32_t)m);
        if (itName != memberNames.end()) name = itName->second;

        uint32_t offset = 0;
        auto itOff = memberOffsets.find((uint32_t)m);
        if (itOff != memberOffsets.end()) offset = itOff->second;

        uint32_t size = typeSize(memberTypeIds[m]);

        if (!name.empty()) {
            outLayout[name] = {offset, size};
        }
        outBlockSize = std::max(outBlockSize, offset + size);
    }

    // Round up to vec4 alignment
    outBlockSize = (outBlockSize + 15) & ~15u;
}

// ──── GLSL → Vulkan GLSL transformation ────

// Transform OpenGL-compatible GLSL to Vulkan-compatible GLSL:
// 1. Change #version 330 core → #version 450
// 2. Remove non-sampler uniform declarations, wrap them in a uniform block
// 3. Add layout(binding=N) to sampler uniforms
// 4. Prefix references to non-sampler uniforms with "_ub."

struct UniformDecl {
    std::string type;       // e.g. "mat4", "vec3", "sampler2D"
    std::string name;       // e.g. "u_projMat"
    std::string extra;      // array size, e.g. "[3]"
    bool isSampler;
    size_t lineStart;       // position of the 'u' in "uniform" in source
    size_t lineEnd;         // position after the semicolon
};

// Find all uniform declarations in GLSL source.
static std::vector<UniformDecl> findUniforms(const std::string& src) {
    std::vector<UniformDecl> result;
    size_t pos = 0;
    while (true) {
        size_t uPos = src.find("\nuniform ", pos);
        if (uPos == std::string::npos) {
            // Also check right at start of file
            if (pos == 0 && src.compare(0, 8, "uniform ") == 0) {
                uPos = 0;
            } else {
                break;
            }
        }
        // uPos points to '\n' or 0; decl starts after "uniform "
        size_t declStart = (uPos == 0) ? 8 : uPos + 9;
        size_t semicolon = src.find(';', declStart);
        if (semicolon == std::string::npos) break;

        std::string decl = src.substr(declStart, semicolon - declStart);
        // Trim trailing spaces
        while (!decl.empty() && (decl.back() == ' ' || decl.back() == '\r'))
            decl.pop_back();

        // Tokenize
        std::vector<std::string> tokens;
        std::istringstream iss(decl);
        std::string tok;
        while (iss >> tok) tokens.push_back(tok);
        if (tokens.empty()) { pos = semicolon + 1; continue; }

        // Last token is name (possibly with array brackets)
        std::string lastName = tokens.back();
        std::string name = lastName;
        std::string extra;
        size_t bracket = name.find('[');
        if (bracket != std::string::npos) {
            extra = name.substr(bracket);
            name = name.substr(0, bracket);
        }

        // Check if sampler
        bool isSampler = false;
        for (auto& t : tokens) {
            if (t.find("sampler") != std::string::npos) { isSampler = true; break; }
        }

        // Build type string (all tokens except last)
        std::string type;
        for (size_t i = 0; i < tokens.size() - 1; ++i) {
            if (i > 0) type += ' ';
            type += tokens[i];
        }

        // Line start: go back to the 'u' of "uniform" (or the beginning of the line)
        size_t lineStart = (uPos == 0) ? 0 : uPos + 1;  // skip the \n

        UniformDecl ud;
        ud.type = type;
        ud.name = name;
        ud.extra = extra;
        ud.isSampler = isSampler;
        ud.lineStart = lineStart;
        ud.lineEnd = semicolon + 1;
        result.push_back(ud);

        pos = semicolon + 1;
    }
    return result;
}

// ──── SPIR-V compilation via shaderc ────

std::vector<uint32_t> VkShader::compileGLSL(const std::string& src,
                                             VkShaderStageFlagBits stage) {
    static shaderc_compiler_t sCompiler = shaderc_compiler_initialize();
    static shaderc_compile_options_t sOptions = []() {
        auto opts = shaderc_compile_options_initialize();
        shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan,
                                                shaderc_env_version_vulkan_1_2);
        return opts;
    }();

    shaderc_shader_kind kind = (stage == VK_SHADER_STAGE_VERTEX_BIT)
        ? shaderc_glsl_vertex_shader : shaderc_glsl_fragment_shader;

    auto t0 = std::chrono::steady_clock::now();
    shaderc_compilation_result_t result = shaderc_compile_into_spv(
        sCompiler, src.c_str(), src.size(), kind, "shader", "main", sOptions);
    auto elapsed = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - t0).count();

    const char* stageStr = (stage == VK_SHADER_STAGE_VERTEX_BIT) ? "vert" : "frag";
    std::vector<uint32_t> spirv;
    if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
        MMD_ERROR("VULKAN", "Shader compile error (%s, %.1fms):\n%s",
                  stageStr, elapsed, shaderc_result_get_error_message(result));
    } else {
        size_t len = shaderc_result_get_length(result);
        spirv.assign((const uint32_t*)shaderc_result_get_bytes(result),
                     (const uint32_t*)(shaderc_result_get_bytes(result) + len));
    }
    shaderc_result_release(result);
    return spirv;
}

// ──── VkShader ────

VkShader::VkShader(VulkanDevice* device, const std::string& vertexSrc,
                   const std::string& fragmentSrc)
    : mDevice(device) {

    // Compile GLSL to SPIR-V directly (source is already Vulkan-compatible)
    auto vsSpirv = compileGLSL(vertexSrc, VK_SHADER_STAGE_VERTEX_BIT);
    auto fsSpirv = compileGLSL(fragmentSrc, VK_SHADER_STAGE_FRAGMENT_BIT);
    MMD_DEBUG("VULKAN", "UBO: %u bytes, %zu members", mUniformSize, mUniformLayout.size());

    if (vsSpirv.empty() || fsSpirv.empty()) {
        MMD_ERROR("VULKAN", "Failed to compile shader to SPIR-V");
        return;
    }

    // Create shader modules
    VkShaderModuleCreateInfo modCI{};
    modCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    modCI.codeSize = vsSpirv.size() * 4;
    modCI.pCode = vsSpirv.data();
    vkCreateShaderModule(mDevice->device(), &modCI, nullptr, &mVsModule);

    modCI.codeSize = fsSpirv.size() * 4;
    modCI.pCode = fsSpirv.data();
    vkCreateShaderModule(mDevice->device(), &modCI, nullptr, &mFsModule);

    // Parse uniform layout from vertex SPIR-V.
    // (Vertex and fragment shaders use the same combined block, so one parse is enough.)
    uint32_t blockSize = 0;
    parseSpirvUniformBlock(vsSpirv.data(), vsSpirv.size(), mUniformLayout, blockSize);

    mUniformSize = blockSize;
    if (mUniformSize == 0) mUniformSize = 16;
    mUniformData.resize(mUniformSize, 0);

    MMD_DEBUG("VULKAN", "Uniform block: %u bytes, %zu members", mUniformSize, mUniformLayout.size());

    // Create descriptor set layout
    // binding 0: uniform buffer
    // bindings 1-6: combined image samplers for texture slots
    constexpr int kMaxTexBindings = 6;
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    if (mUniformSize > 0) {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(uboBinding);
    }

    for (int i = 0; i < kMaxTexBindings; ++i) {
        VkDescriptorSetLayoutBinding texBinding{};
        texBinding.binding = i + 1;
        texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        texBinding.descriptorCount = 1;
        texBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(texBinding);
    }

    VkDescriptorSetLayoutCreateInfo dslCI{};
    dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslCI.bindingCount = (uint32_t)bindings.size();
    dslCI.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(mDevice->device(), &dslCI, nullptr,
                                     &mDescSetLayout) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create descriptor set layout");
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount = 1;
    plCI.pSetLayouts = &mDescSetLayout;

    if (vkCreatePipelineLayout(mDevice->device(), &plCI, nullptr,
                                &mPipelineLayout) != VK_SUCCESS) {
        MMD_ERROR("VULKAN", "Failed to create pipeline layout");
    }
}

VkShader::~VkShader() {
    destroy();
}

void VkShader::destroy() {
    if (mPipelineLayout) {
        vkDestroyPipelineLayout(mDevice->device(), mPipelineLayout, nullptr);
        mPipelineLayout = VK_NULL_HANDLE;
    }
    if (mDescSetLayout) {
        vkDestroyDescriptorSetLayout(mDevice->device(), mDescSetLayout, nullptr);
        mDescSetLayout = VK_NULL_HANDLE;
    }
    if (mFsModule) {
        vkDestroyShaderModule(mDevice->device(), mFsModule, nullptr);
        mFsModule = VK_NULL_HANDLE;
    }
    if (mVsModule) {
        vkDestroyShaderModule(mDevice->device(), mVsModule, nullptr);
        mVsModule = VK_NULL_HANDLE;
    }
}

void VkShader::use() {
    mDevice->setCurrentShader(this);
}

uint32_t VkShader::flushUniforms(void* dst, uint32_t maxSize) const {
    uint32_t size = std::min(mUniformSize, maxSize);
    memcpy(dst, mUniformData.data(), size);
    return size;
}

// ──── Uniform setters ────

void VkShader::setInt(const std::string& name, int value) {
    auto it = mUniformLayout.find(name);
    if (it != mUniformLayout.end() && it->second.offset + 4 <= mUniformSize) {
        memcpy(mUniformData.data() + it->second.offset, &value, 4);
    }
}

void VkShader::setFloat(const std::string& name, float value) {
    auto it = mUniformLayout.find(name);
    if (it != mUniformLayout.end() && it->second.offset + 4 <= mUniformSize) {
        memcpy(mUniformData.data() + it->second.offset, &value, 4);
    }
}

void VkShader::setVec2(const std::string& name, float x, float y) {
    auto it = mUniformLayout.find(name);
    if (it != mUniformLayout.end() && it->second.offset + 8 <= mUniformSize) {
        float* dst = (float*)(mUniformData.data() + it->second.offset);
        dst[0] = x; dst[1] = y;
    }
}

void VkShader::setVec3(const std::string& name, float x, float y, float z) {
    auto it = mUniformLayout.find(name);
    if (it != mUniformLayout.end() && it->second.offset + 12 <= mUniformSize) {
        float* dst = (float*)(mUniformData.data() + it->second.offset);
        dst[0] = x; dst[1] = y; dst[2] = z;
    }
}

void VkShader::setVec4(const std::string& name, float x, float y, float z, float w) {
    auto it = mUniformLayout.find(name);
    if (it != mUniformLayout.end() && it->second.offset + 16 <= mUniformSize) {
        float* dst = (float*)(mUniformData.data() + it->second.offset);
        dst[0] = x; dst[1] = y; dst[2] = z; dst[3] = w;
    }
}

void VkShader::setMat4(const std::string& name, const float* data) {
    auto it = mUniformLayout.find(name);
    if (it != mUniformLayout.end() && it->second.offset + 64 <= mUniformSize) {
        memcpy(mUniformData.data() + it->second.offset, data, 64);
    }
}

}  // namespace Gpu
