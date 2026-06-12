#pragma once

#include "anim/MorphController.h"
#include "pmx/PmxModel.h"

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
    std::vector<float> edgeFactors;
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
        const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses);
    static std::vector<std::array<float, 16>> computePoseWorldMatrices(
        const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses,
        const std::unordered_map<
            std::string, std::pair<std::array<float, 3>, std::array<float, 4>>>& vmdTransforms);

    static std::vector<float> computeSkinningMatrices(const PmxModel& model);
    static std::vector<float> computeSkinningMatrices(
        const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses);
    static std::vector<float> computeSkinningMatrices(
        const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses,
        const std::unordered_map<std::string, VmdBoneTransform>& vmdTransforms);
    static std::vector<float> computeSkinningMatrices(
        const PmxModel& model, const std::vector<std::array<float, 16>>& poseWorld);

    // Recompute bones with BONEFLAG_IS_AFTER_PHYSICS_DEFORM after physics step
    static void recomputeAfterPhysicsBones(const PmxModel& model,
                                           const std::unordered_map<std::string, VpdPose>& vpdPoses,
                                           std::vector<std::array<float, 16>>& poseWorld);

    // Overlay physics world matrices onto skinning matrices
    static void applyPhysics(const PmxModel& model, std::vector<float>& skinMatrices,
                             const std::vector<std::array<float, 16>>& physicsMats);

    static void applyBoneMorphs(std::vector<float>& skinMatrices, int boneCount,
                                 const std::unordered_map<int, BoneMorphTransform>& boneMorphs,
                                 float modelScale);

    static BoneTextureData packBoneMatrices(const std::vector<float>& matrices, int numBones);
};
