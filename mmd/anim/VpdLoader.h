#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

struct VpdPose {
    float tx = 0, ty = 0, tz = 0;
    float qx = 0, qy = 0, qz = 0, qw = 1;

    void toMatrix(float out[9]) const;
};

class VpdLoader {
   public:
    static std::unordered_map<std::string, VpdPose> load(const std::filesystem::path& path);
};
