#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/util/Log.h"

namespace Gpu {
class IGpuDevice;
}

namespace mmd {

// Backend selection for mmd::init().
enum class GpuBackend {
    OpenGL,    // default
    // Vulkan,  // future
};

// Initialization arguments passed to mmd::init().
struct InitArgs {
    std::filesystem::path shaderDir;
    std::filesystem::path toonDir;
    std::filesystem::path effectsCfg;
    std::vector<std::string> blinkMorphs;  // morph names for auto-blink, set by caller
    LogFunc logFunc = nullptr;             // optional custom log sink
    GpuBackend backend = GpuBackend::OpenGL;
};

// Initialize mmd module (GPU resources, etc.). Must call while GL context is alive.
void init(const InitArgs& args);

// Release all global resources (GPU then CPU). Call before GL context destroyed.
void dispose();

// Access stored init arguments (read-only).
const InitArgs& initArgs();

// Access the current GPU device (convenience, equivalent to Gpu::device()).
Gpu::IGpuDevice* gpuDevice();

}  // namespace mmd
