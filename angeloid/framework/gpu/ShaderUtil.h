#pragma once

#include "core/util/Log.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace Gpu {

inline std::string readShaderFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        MMD_ERROR("SHADER", "Failed to open shader: %s", path.string().c_str());
        return {};
    }
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

}  // namespace Gpu
