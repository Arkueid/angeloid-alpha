#pragma once

#include "pmx/PmxModel.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct BoneMorphTransform {
    std::array<float, 3> translation = {0, 0, 0};
    std::array<float, 4> rotation = {0, 0, 0, 1};
};

struct MatMorphOverride {
    float alpha = 1.0f;
    Vec3 diffuse = {1, 1, 1};
    Vec3 specular = {1, 1, 1};
    Vec3 ambient = {1, 1, 1};
    float specularFactor = 1.0f;
    Vec4 edgeColor = {0, 0, 0, 0};  // additive offset
    float edgeSize = 0;
};

class MorphController {
public:
    MorphController();

    void setModel(const PmxModel& model);

    void setMorphWeight(const std::string& name, float weight);
    void setMorphWeights(const std::unordered_map<std::string, float>& weights);
    void clearMorphs();

    float getMaterialAlpha(int materialIndex, float originalAlpha) const;
    const MatMorphOverride* getMaterialOverride(int materialIndex) const;
    bool hasActiveMorphs() const { return !mMorphWeights.empty(); }

    std::unordered_map<std::string, float>& morphWeights() { return mMorphWeights; }
    void updateMorphOffsets();
    bool offsetsChanged() const { return mOffsetsDirty; }
    void clearOffsetsChanged() { mOffsetsDirty = false; }

    const std::unordered_map<int, BoneMorphTransform>& boneMorphs() const { return mBoneMorphs; }
    const std::vector<float>& positionOffsets() const { return mPosOffsets; }
    const std::vector<float>& uvOffsets() const { return mUvOffsets; }

private:
    const PmxModel* mModel = nullptr;
    std::unordered_map<std::string, float> mMorphWeights;
    std::unordered_map<int, MatMorphOverride> mMaterialOverrides;
    std::unordered_map<int, BoneMorphTransform> mBoneMorphs;

    std::unordered_map<std::string, int> mMorphNameIndex;

    std::vector<float> mPosOffsets;
    std::vector<float> mUvOffsets;
    bool mOffsetsDirty = true;
    bool mLastHadActive = false;
};
