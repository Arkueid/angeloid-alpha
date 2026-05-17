#pragma once

#include "Gpu/Mesh.h"
#include "pmx/PmxModel.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Gpu { class Texture; }

struct BoneMorphTransform {
    std::array<float, 3> translation = {0, 0, 0};
    std::array<float, 4> rotation = {0, 0, 0, 1};
};

struct MatMorphOverride {
    float alpha = 1.0f;
    Vec3 diffuse = {1, 1, 1};    // multiplier
    Vec3 specular = {1, 1, 1};   // multiplier
    Vec3 ambient = {1, 1, 1};    // multiplier
    float specularFactor = 1.0f;
};

class MorphController {
public:
    MorphController();

    void setModel(const PmxModel& model,
                  Gpu::VboWrapper* morphVbo,
                  Gpu::VboWrapper* uvMorphVbo,
                  float modelScale);

    void setMorphWeight(const std::string& name, float weight);
    void setMorphWeights(const std::unordered_map<std::string, float>& weights);
    void clearMorphs();

    float getMaterialAlpha(int materialIndex, float originalAlpha) const;
    const MatMorphOverride* getMaterialOverride(int materialIndex) const;
    bool hasActiveMorphs() const { return !mMorphWeights.empty(); }

    std::unordered_map<std::string, float>& morphWeights() { return mMorphWeights; }
    void updateMorphOffsets();

    const std::unordered_map<int, BoneMorphTransform>& boneMorphs() const { return mBoneMorphs; }

    // Quaternion slerp (used by applyMorphRecursive)
    static std::array<float, 4> slerpQuat(const std::array<float, 4>& a,
                                           const std::array<float, 4>& b, float t);

private:
    const PmxModel* mModel = nullptr;
    Gpu::VboWrapper* mMorphVbo = nullptr;
    Gpu::VboWrapper* mUvMorphVbo = nullptr;
    float mModelScale = 1.0f;

    std::unordered_map<std::string, float> mMorphWeights;
    std::unordered_map<int, MatMorphOverride> mMaterialOverrides;
    std::unordered_map<int, BoneMorphTransform> mBoneMorphs;

    std::vector<float> mPosOffsets;
    std::vector<float> mUvOffsets;
};
