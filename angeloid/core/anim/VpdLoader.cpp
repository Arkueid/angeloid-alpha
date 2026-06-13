#include "core/anim/VpdLoader.h"

#include "core/encoding/Encoding.h"
#include "core/util/Log.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

void VpdPose::toMatrix(float out[9]) const {
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, yz = qy * qz, xz = qx * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;

    out[0] = 1.0f - 2.0f * (yy + zz);
    out[1] = 2.0f * (xy - wz);
    out[2] = 2.0f * (xz + wy);
    out[3] = 2.0f * (xy + wz);
    out[4] = 1.0f - 2.0f * (xx + zz);
    out[5] = 2.0f * (yz - wx);
    out[6] = 2.0f * (xz - wy);
    out[7] = 2.0f * (yz + wx);
    out[8] = 1.0f - 2.0f * (xx + yy);
}

// VPD (Vocaloid Pose Data) format — a text-based bone pose file.
// Format per bone:
//   Bone{bone_name
//       tx,ty,tz;       // translation (comma-separated)
//       qx,qy,qz,qw;    // rotation quaternion (comma-separated)
//   }
// Encoding may be Shift-JIS or UTF-8; auto-detected via isValidUtf8().
// Comments (//...) and trailing semicolons are stripped.
std::unordered_map<std::string, VpdPose> VpdLoader::load(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        MMD_ERROR("VPD", "VPD file not found: %s", path.string().c_str());
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        MMD_ERROR("VPD", "Failed to open VPD: %s", path.string().c_str());
        return {};
    }

    std::stringstream buf;
    buf << file.rdbuf();
    std::string raw = buf.str();

    std::string text = Encoding::isValidUtf8(raw) ? raw : Encoding::cp932ToUtf8(raw);

    std::unordered_map<std::string, VpdPose> poses;
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos)
            continue;
        line = line.substr(start);

        if (line.rfind("Bone", 0) != 0)
            continue;

        size_t braceOpen = line.find('{');
        if (braceOpen == std::string::npos)
            continue;

        std::string boneName = line.substr(braceOpen + 1);
        while (!boneName.empty() &&
               (boneName.back() == ' ' || boneName.back() == '\t' || boneName.back() == '\r'))
            boneName.pop_back();

        if (!std::getline(stream, line))
            break;
        size_t commentPos = line.find("//");
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);
        line.erase(std::remove(line.begin(), line.end(), ';'), line.end());

        float tx = 0, ty = 0, tz = 0;
        sscanf(line.c_str(), "%f,%f,%f", &tx, &ty, &tz);

        if (!std::getline(stream, line))
            break;
        commentPos = line.find("//");
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);
        line.erase(std::remove(line.begin(), line.end(), ';'), line.end());

        float qx = 0, qy = 0, qz = 0, qw = 1;
        sscanf(line.c_str(), "%f,%f,%f,%f", &qx, &qy, &qz, &qw);

        std::getline(stream, line);  // skip closing brace

        poses[boneName] = {tx, ty, tz, qx, qy, qz, qw};
    }
    return poses;
}
