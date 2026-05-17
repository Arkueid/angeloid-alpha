#pragma once

#include "pmx/PmxModel.h"

#include "Gpu/Texture.h"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct SkinningVertexData {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    std::vector<int32_t> boneIndices;
    std::vector<float> boneWeights;
};

struct VpdPose;

// VMD bone transform: (position vec3, rotation quat)
using VmdBoneTransform = std::pair<std::array<float, 3>, std::array<float, 4>>;

struct BoneTextureData {
    std::vector<float> pixels;
    int width;
    int height;
};

struct BoneSkinning {
    static SkinningVertexData extractSkinningData(const PmxModel& model);

    static std::vector<std::array<float, 16>> computeBindWorldMatrices(const PmxModel& model);
    static std::vector<std::array<float, 16>> computePoseWorldMatrices(const PmxModel& model);
    static std::vector<std::array<float, 16>> computePoseWorldMatrices(
        const PmxModel& model,
        const std::unordered_map<std::string, VpdPose>& vpdPoses);

    static std::vector<float> computeSkinningMatrices(
        const PmxModel& model, const Vec3& center, float minY, float scale);
    static std::vector<float> computeSkinningMatrices(
        const PmxModel& model, const Vec3& center, float minY, float scale,
        const std::unordered_map<std::string, VpdPose>& vpdPoses);
    static std::vector<float> computeSkinningMatrices(
        const PmxModel& model, const Vec3& center, float minY, float scale,
        const std::unordered_map<std::string, VpdPose>& vpdPoses,
        const std::unordered_map<std::string, VmdBoneTransform>& vmdTransforms);

    static BoneTextureData packBoneMatrices(const std::vector<float>& matrices, int numBones);
    static std::unique_ptr<Gpu::Texture> createBoneTexture(const BoneTextureData& data);
};
