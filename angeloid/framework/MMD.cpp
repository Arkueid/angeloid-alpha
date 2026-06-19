#include "framework/MMD.h"
#include "core/util/Log.h"
#include "framework/Pipeline.h"
#include "framework/RenderContext.h"
#include "framework/ShaderManager.h"
#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/opengl/GlDevice.h"
#ifdef MMD_VULKAN_BACKEND
#include "framework/gpu/vulkan/VkDevice.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include <cstdio>   // printf, fprintf, vprintf, vfprintf

#ifdef MMD_ENABLE_STACKTRACE
#include "backward.hpp"
#endif

namespace mmd {
namespace {

// Default printf-based logger. Registered in init() if user provides no custom LogFunc.
void defaultLogFunc(LogLevel level, const char* module,
                    const char* fmt, va_list args) {
    switch (level) {
    case LogLevel::Error:
        fprintf(stderr, "[ERROR][%s] ", module);
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        break;
    case LogLevel::Warn:
        printf("[WARN][%s] ", module);
        vprintf(fmt, args);
        printf("\n");
        break;
    case LogLevel::Debug:
        printf("[DEBUG][%s] ", module);
        vprintf(fmt, args);
        printf("\n");
        break;
    case LogLevel::Info:
    default:
        printf("[INFO][%s] ", module);
        vprintf(fmt, args);
        printf("\n");
        break;
    }
}

}  // namespace

static InitArgs sInitArgs;

void init(const InitArgs& args) {
    sInitArgs = args;
    setLogFunc(args.logFunc ? args.logFunc : defaultLogFunc);
#ifdef MMD_ENABLE_STACKTRACE
    static backward::SignalHandling sh;
#endif

    // Create GPU device
    if (args.backend == GpuBackend::Vulkan) {
#ifdef MMD_VULKAN_BACKEND
        Gpu::setDevice(std::make_unique<Gpu::VulkanDevice>(args.window));
#else
        MMD_ERROR("GPU", "Vulkan backend not compiled in");
#endif
    } else {
        Gpu::setDevice(std::make_unique<Gpu::GlDevice>());
    }

    RenderContext::instance().init(args.toonDir);
    ShaderManager::instance().init(args.effectsCfg, args.shaderDir);
    Pipeline::instance().init();
}

void dispose() {
    Pipeline::instance().clear();
    ShaderManager::instance().clear();
    RenderContext::instance().release();
    Gpu::setDevice(nullptr);  // destroy GPU device last
}

const InitArgs& initArgs() {
    return sInitArgs;
}

Gpu::IGpuDevice* gpuDevice() {
    return Gpu::device();
}

}  // namespace mmd
