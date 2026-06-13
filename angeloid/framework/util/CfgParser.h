#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

// Minimal key=value config parser.
//   key = value          — flat format (parseCfgFile)
//   [section]            — section-based format (parseCfgSections)
// Lines starting with # are comments; blank lines are ignored.

namespace {

inline void trim(std::string& s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    s.erase(0, i);
}

inline bool skipLine(const std::string& line, size_t& start) {
    start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
        ++start;
    return start >= line.size() || line[start] == '#';
}

inline bool parsePair(const std::string& line, size_t start,
                      std::string& key, std::string& val) {
    auto eq = line.find('=', start);
    if (eq == std::string::npos) return false;
    key = line.substr(start, eq - start);
    val = line.substr(eq + 1);
    trim(key);
    trim(val);
    return !key.empty() && !val.empty();
}

} // namespace

inline std::unordered_map<std::string, std::string>
parseCfgFile(const std::filesystem::path& path) {
    std::unordered_map<std::string, std::string> kv;
    std::ifstream f(path);
    if (!f.is_open()) return kv;

    std::string line;
    while (std::getline(f, line)) {
        trim(line);
        size_t start;
        if (skipLine(line, start)) continue;

        std::string key, val;
        if (parsePair(line, start, key, val))
            kv[key] = val;
    }
    return kv;
}

inline std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
parseCfgSections(const std::filesystem::path& path) {
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> result;
    std::ifstream f(path);
    if (!f.is_open()) return result;

    std::string section, line;
    while (std::getline(f, line)) {
        trim(line);
        size_t start;
        if (skipLine(line, start)) continue;

        if (line[start] == '[') {
            auto end = line.find(']', start + 1);
            if (end != std::string::npos) {
                section = line.substr(start + 1, end - start - 1);
                trim(section);
            }
            continue;
        }

        if (section.empty()) continue;

        std::string key, val;
        if (parsePair(line, start, key, val))
            result[section][key] = val;
    }
    return result;
}
