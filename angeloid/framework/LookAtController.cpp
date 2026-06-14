#include "framework/LookAtController.h"

#include "core/pmx/PmxModel.h"
#include "core/util/Log.h"
#include "framework/Camera.h"

#include <cmath>

namespace mmd {

void LookAtController::setup(const PmxModel& pmx) {
    mHeadBoneIndex = mNeckBoneIndex = -1;
    mLeftEyeBoneIndex = mRightEyeBoneIndex = -1;
    mBoneChildren.clear();
    mBoneChildren.resize(pmx.boneCount());

    for (int i = 0; i < pmx.boneCount(); ++i) {
        const auto& bone = pmx.bones[i];
        const auto& name = bone.name;
        const auto& ename = bone.english_name;

        if (name == reinterpret_cast<const char*>(u8"頭") || ename == "head")
            mHeadBoneIndex = i;
        else if (name == reinterpret_cast<const char*>(u8"首") || ename == "neck")
            mNeckBoneIndex = i;
        else if ((name == reinterpret_cast<const char*>(u8"左目") || ename == "left eye") &&
                 mLeftEyeBoneIndex < 0)
            mLeftEyeBoneIndex = i;
        else if ((name == reinterpret_cast<const char*>(u8"右目") || ename == "right eye") &&
                 mRightEyeBoneIndex < 0)
            mRightEyeBoneIndex = i;

        int p = bone.parent_index;
        if (p >= 0 && p < pmx.boneCount())
            mBoneChildren[p].push_back(i);
    }

    if (mHeadBoneIndex >= 0)
        MMD_INFO("MODEL", "LookAt bones: head=%d neck=%d leftEye=%d rightEye=%d",
                 mHeadBoneIndex, mNeckBoneIndex, mLeftEyeBoneIndex, mRightEyeBoneIndex);
}

void LookAtController::start(int screenX, int screenY, int screenW, int screenH) {
    mEnabled = true;
    mScreenX = screenX;
    mScreenY = screenY;
    mScreenW = screenW > 0 ? screenW : 1;
    mScreenH = screenH > 0 ? screenH : 1;
}

void LookAtController::reset() {
    mEnabled = false;
}

void LookAtController::propagateToDescendants(
    std::vector<std::array<float, 16>>& poseWorld,
    int parentIdx, const Mat4& deltaWorld) const {
    std::vector<int> stack;
    for (int c : mBoneChildren[parentIdx])
        stack.push_back(c);
    while (!stack.empty()) {
        int child = stack.back();
        stack.pop_back();
        poseWorld[child] = mat4Mul(deltaWorld, poseWorld[child]);
        for (int gc : mBoneChildren[child])
            stack.push_back(gc);
    }
}

void LookAtController::applyBoneQuat(std::vector<std::array<float, 16>>& poseWorld,
                                     int boneIdx, const Quat& qFull, float angleScale) const {
    if (boneIdx < 0 || boneIdx >= (int)poseWorld.size())
        return;

    for (int j = 0; j < 16; ++j)
        if (std::isnan(poseWorld[boneIdx][j]))
            return;

    float halfAngle = std::acos(std::max(-1.0f, std::min(1.0f, qFull.w)));
    if (halfAngle < 1e-6f)
        return;

    float angle = 2.0f * halfAngle;
    float scaledAngle = angle * angleScale;
    if (scaledAngle < 1e-6f)
        return;

    static constexpr float kMaxAngle = 50.0f * 3.14159265f / 180.0f;
    if (scaledAngle > kMaxAngle * 1.5f)
        scaledAngle = kMaxAngle * 1.5f;

    float sinHalf = std::sin(halfAngle);
    float ratio = std::sin(scaledAngle * 0.5f) / sinHalf;
    Quat q = quatNormalize(
        {qFull.x * ratio, qFull.y * ratio, qFull.z * ratio, std::cos(scaledAngle * 0.5f)});

    // Apply world-space rotation around bone's pivot
    Mat4 R = mat4FromQuat(q);
    Mat4 oldWorld = poseWorld[boneIdx];
    Mat4 newWorld = mat4Mul(R, oldWorld);
    newWorld[12] = oldWorld[12];
    newWorld[13] = oldWorld[13];
    newWorld[14] = oldWorld[14];
    poseWorld[boneIdx] = newWorld;

    Mat4 deltaWorld = mat4Mul(newWorld, mat4InverseAffine(oldWorld));
    propagateToDescendants(poseWorld, boneIdx, deltaWorld);
}

void LookAtController::apply(std::vector<std::array<float, 16>>& poseWorld,
                             const std::array<float, 16>& modelMat) {
    if (mHeadBoneIndex < 0 || mScreenW <= 0 || mScreenH <= 0)
        return;

    auto& cam = Camera::instance();
    Vec3 camPos;
    cam.getEyePosition(camPos.x, camPos.y, camPos.z);
    auto view = cam.viewMatrix();
    Vec3 right = {view[0], view[4], view[8]};
    Vec3 upVec = {view[1], view[5], view[9]};
    Vec3 backward = {view[2], view[6], view[10]};

    // Head world position
    Vec3 headPosModel = mat4Translation(poseWorld[mHeadBoneIndex]);
    const float* mm = modelMat.data();
    Mat4 mm4 = {mm[0], mm[1], mm[2],  mm[3],  mm[4],  mm[5],  mm[6],  mm[7],
                mm[8], mm[9], mm[10], mm[11], mm[12], mm[13], mm[14], mm[15]};
    Vec3 headPosWorld = mat4TransformPoint(mm4, headPosModel);

    // Project head to NDC
    float aspect = (float)mScreenW / (float)mScreenH;
    float tanHalfFov = std::tan(45.0f * 3.14159265f / 360.0f);
    Vec3 headView = {vec3Dot(vec3Sub(headPosWorld, camPos), right),
                     vec3Dot(vec3Sub(headPosWorld, camPos), upVec),
                     vec3Dot(vec3Sub(headPosWorld, camPos), backward)};
    float headViewZ = -headView.z;
    if (headViewZ < 0.1f)
        headViewZ = 5.0f;
    float headNdcX = -(headView.x / headViewZ) / (tanHalfFov * aspect);
    float headNdcY = (headView.y / headViewZ) / tanHalfFov;

    // Mouse NDC
    float mouseNdcX = 1.0f - (2.0f * mScreenX) / mScreenW;
    float mouseNdcY = 1.0f - (2.0f * mScreenY) / mScreenH;

    static constexpr float kMaxAngle = 50.0f * 3.14159265f / 180.0f;

    if (cam.mode() == CameraMode::Orbit)
        return;

    // FPS mode: NDC-based mouse tracking
    float relNdcX = mouseNdcX - headNdcX;
    float relNdcY = mouseNdcY - headNdcY;
    Quat qYaw = quatFromAxisAngle(upVec, relNdcX * kMaxAngle);
    Quat qPitch = quatFromAxisAngle(right, relNdcY * kMaxAngle);
    Quat qFull = quatNormalize(quatMul(qPitch, qYaw));

    applyBoneQuat(poseWorld, mNeckBoneIndex, qFull, 0.1f);
    applyBoneQuat(poseWorld, mHeadBoneIndex, qFull, 1.0f);
    applyBoneQuat(poseWorld, mLeftEyeBoneIndex, qFull, 0.2f);
    applyBoneQuat(poseWorld, mRightEyeBoneIndex, qFull, 0.2f);
}

} // namespace mmd
