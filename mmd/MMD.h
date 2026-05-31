#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mmd {

// Initialization arguments passed to mmd::init().
struct InitArgs {
    std::filesystem::path shaderDir;
    std::filesystem::path toonDir;
    std::vector<std::string> blinkMorphs;  // morph names for auto-blink, set by caller
};

// Initialize mmd module (GPU resources, etc.). Must call while GL context is alive.
void init(const InitArgs& args);

// Initialize OpenGL loader (glad). Must call after GL context is created.
void glInit();

// Release all global resources (GPU then CPU). Call before GL context destroyed.
void dispose();

// Access stored init arguments (read-only).
const InitArgs& initArgs();

}  // namespace mmd
