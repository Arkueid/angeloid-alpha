#pragma once

#include "Camera.h"
#include "anim/BoneSkinning.h"
#include "anim/MorphController.h"
#include "anim/PhysicsWorld.h"
#include "anim/VmdPlayer.h"
#include "anim/VpdLoader.h"
#include "pmx/PmxModel.h"
#include "render/opengl/ModelRenderer.h"
#include "render/opengl/debug/RigidBodyRenderer.h"

#include <array>
#include <filesystem>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmd {

class Model {
public:
    Model() = default;

    // --- Loading ---
    void load(const std::filesystem::path& pmxPath);

    // --- Per-frame ---
    void update(float dt);
    void draw(int screenWidth, int screenHeight);

    // --- Physics ---
    void enablePhysics(bool on);
    bool physicsEnabled() const {
        return mPhysics.enabled;
    }
    void showRigidBodies(bool v) {
        mShowRigidBodies = v;
    }

    // --- VMD animation ---
    int  loadVmd(const std::filesystem::path& path);
    void playVmd(int trackId, std::function<void(int)> onEnd = nullptr);
    void pauseVmd(int trackId);
    void stopVmd(int trackId);
    void removeVmd(int trackId);

    void playAllVmd();
    void pauseAllVmd();
    void stopAllVmd();
    bool isVmdPlaying() const;

    int   vmdTrackCount() const;
    bool  isVmdPlaying(int trackId) const;
    float vmdCurrentFrame(int trackId) const;
    float vmdMaxFrame(int trackId) const;
    void  setVmdFrame(int trackId, float frame);

    // --- VPD pose ---
    int  loadVpd(const std::filesystem::path& path);
    void applyVpd(int vpdId);
    void resetPose();         // clear VPD + VMD, back to bind pose
    void syncVpdPose();       // recompute pose from active VPD, discarding VMD
    bool vpdApplied() const {
        return mActiveVpdId >= 0;
    }
    void removeVpd(int vpdId);

    // --- Display toggles ---
    void showModel(bool v) {
        mRenderer.showModel = v;
    }
    void showOutline(bool v) {
        mRenderer.showOutline = v;
    }
    void showToon(bool v) {
        mRenderer.showToon = v;
    }
    bool showModel() const {
        return mRenderer.showModel;
    }
    bool showOutline() const {
        return mRenderer.showOutline;
    }
    bool showToon() const {
        return mRenderer.showToon;
    }
    // --- LookAt ---
    void lookAt(int screenX, int screenY, int screenW, int screenH);
    void resetLookAt();

    // --- Morphs ---
    void setMorphWeight(const std::string& name, float weight);
    float savedMorphWeight(const std::string& name) const;
    void clearMorphs();
    void setMorphWeights(const std::unordered_map<std::string, float>& weights);
    void setIdleBlink(bool on) {
        mIdleEnabled = on;
    }
    int morphCount() const;
    std::vector<int> interactableMorphs() const;
    std::string morphName(int index) const;

    // --- Accessors ---
    const std::string& modelName() const {
        return mPmx.name;
    }
    const PmxModel& data() const {
        return mPmx;
    }
    float modelScale() const {
        return mRenderer.modelScale();
    }
    const float* modelMatrix() const {
        return mRenderer.modelMatrix();
    }

private:
    void syncBoneTexture();
    void syncMorphOffsets();

    PmxModel mPmx;
    ModelRenderer mRenderer;
    PhysicsWorld mPhysics;
    MorphController mMorphCtl;
    std::unique_ptr<RigidBodyRenderer> mPhysicsDebug;
    std::unique_ptr<VmdMixer> mVmdMixer;

    std::deque<VmdAnimation> mVmdAnimations;
    std::vector<std::pair<int, VpdPoseMap>> mVpdPoses; // (vpdId, poses)
    int mActiveVpdId = -1;
    int mVpdNextId = 1;
    std::filesystem::path mVpdPath;

    std::vector<std::array<float, 16>> mPoseWorld;
    std::vector<std::array<float, 16>> mBindPoseWorld;

    bool mIdleEnabled = true;
    float mIdleTime = 0;
    bool mShowRigidBodies = false;
    bool mClearVmd = false; // if true, skip VMD updates

    std::unordered_map<std::string, float> mSavedWeights;

    std::unordered_map<std::string, std::pair<std::array<float, 3>, std::array<float, 4>>> mVmdBoneCache;
    std::unordered_map<std::string, float> mVmdMorphCache;

    // LookAt state
    int mHeadBoneIndex = -1;
    int mNeckBoneIndex = -1;
    int mLeftEyeBoneIndex = -1;
    int mRightEyeBoneIndex = -1;
    std::vector<std::vector<int>> mBoneChildren;
    bool mLookAtEnabled = false;
    int mLookAtScreenX = 0, mLookAtScreenY = 0;
    int mLookAtScreenW = 1, mLookAtScreenH = 1;


    void applyLookAt();
    void applyBoneQuat(int boneIdx, const Quat& qFull, float angleScale);
    void propagateToDescendants(int parentIdx, const std::array<float, 16>& deltaWorld);
};

}  // namespace mmd
