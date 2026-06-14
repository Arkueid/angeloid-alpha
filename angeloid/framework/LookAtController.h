#pragma once

#include "core/math/VecMath.h"

#include <array>
#include <vector>

struct PmxModel;

namespace mmd {

class LookAtController {
public:
    LookAtController() = default;

    // Detect head/neck/eye bones by name (JP/EN) and build children adjacency.
    // Called once from Model::load().
    void setup(const PmxModel& pmx);

    void start(int screenX, int screenY, int screenW, int screenH);
    void reset();

    bool enabled() const { return mEnabled; }

    // Apply look-at rotation to poseWorld in-place.
    // modelMatrix: model-to-world transform from ModelRenderer (16 floats, column-major).
    void apply(std::vector<std::array<float, 16>>& poseWorld,
               const std::array<float, 16>& modelMatrix);

private:
    void propagateToDescendants(std::vector<std::array<float, 16>>& poseWorld,
                                int parentIdx, const Mat4& deltaWorld) const;
    void applyBoneQuat(std::vector<std::array<float, 16>>& poseWorld,
                       int boneIdx, const Quat& qFull, float angleScale) const;

    int mHeadBoneIndex = -1, mNeckBoneIndex = -1;
    int mLeftEyeBoneIndex = -1, mRightEyeBoneIndex = -1;
    std::vector<std::vector<int>> mBoneChildren;
    bool mEnabled = false;
    int mScreenX = 0, mScreenY = 0, mScreenW = 1, mScreenH = 1;
};

} // namespace mmd
