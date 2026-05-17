#pragma once

#include <cstdint>
#include <string>

struct Encoding {
    static std::string utf16leToUtf8(const uint16_t* data, size_t count);
    static std::string cp932ToUtf8(const std::string& raw);
    static bool isValidUtf8(const std::string& s);
};
