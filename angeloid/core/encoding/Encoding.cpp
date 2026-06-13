#include "core/encoding/Encoding.h"

#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

std::string Encoding::utf16leToUtf8(const uint16_t* data, size_t count) {
    std::string result;
    result.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        uint32_t cp = data[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < count) {
            uint32_t low = data[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        if (cp < 0x80) {
            result += (char)cp;
        }
        else if (cp < 0x800) {
            result += (char)(0xC0 | (cp >> 6));
            result += (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000) {
            result += (char)(0xE0 | (cp >> 12));
            result += (char)(0x80 | ((cp >> 6) & 0x3F));
            result += (char)(0x80 | (cp & 0x3F));
        }
        else {
            result += (char)(0xF0 | (cp >> 18));
            result += (char)(0x80 | ((cp >> 12) & 0x3F));
            result += (char)(0x80 | ((cp >> 6) & 0x3F));
            result += (char)(0x80 | (cp & 0x3F));
        }
    }
    return result;
}

std::string Encoding::cp932ToUtf8(const std::string& raw) {
#ifdef _WIN32
    int wideLen = MultiByteToWideChar(932, 0, raw.data(), (int)raw.size(), nullptr, 0);
    if (wideLen <= 0)
        return raw;
    std::vector<wchar_t> wide(wideLen);
    MultiByteToWideChar(932, 0, raw.data(), (int)raw.size(), wide.data(), wideLen);

    int utf8Len =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0)
        return raw;
    std::string result(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen, result.data(), utf8Len, nullptr, nullptr);
    return result;
#else
    return raw;  // non-Windows: no CP932 support, return as-is
#endif
}

bool Encoding::isValidUtf8(const std::string& s) {
    for (size_t i = 0; i < s.size(); ++i) {
        uint8_t c = (uint8_t)s[i];
        if (c < 0x80)
            continue;
        int trail = 0;
        if ((c & 0xE0) == 0xC0)
            trail = 1;
        else if ((c & 0xF0) == 0xE0)
            trail = 2;
        else if ((c & 0xF8) == 0xF0)
            trail = 3;
        else
            return false;
        for (int j = 1; j <= trail; ++j) {
            if (i + j >= s.size())
                return false;
            if (((uint8_t)s[i + j] & 0xC0) != 0x80)
                return false;
        }
        i += trail;
    }
    return true;
}
