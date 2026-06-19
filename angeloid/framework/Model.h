#pragma once

#include "core/anim/BoneSkinning.h"
#include "core/anim/MorphController.h"
#include "core/anim/PhysicsWorld.h"
#include "core/util/Log.h"
#include "core/pmx/PmxModel.h"
#include "framework/Renderable.h"
#include "framework/ModelRenderer.h"
#include "framework/AnimationController.h"
#include "framework/LookAtController.h"
#include "framework/scene/RigidBodyRenderer.h"

#include <array>
#include <filesystem>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmd {

class Model : public Renderable {
public:
    Model() = default;

    // --- Loading ---
    void load(const std::filesystem::path& pmxPath);

    // --- Per-frame ---
    void update(float dt);

    // --- Renderable ---
    const char* name() const override { return mPmx.name.c_str(); }
    bool castShadow() const override { return true; }
    bool shadowBounds(Vec3& outMin, Vec3& outMax) const override;

    void onShadowPass(const ShadowPassParams& sp) override;
    void onMainPass(const MainPassParams& mp) override;
    void onDebugPass(const DebugPassParams& dp) override;

    // --- Physics ---
    void enablePhysics(bool on);
    bool physicsEnabled() const {
        return mPhysics.enabled;
    }
    void showRigidBodies(bool v) {
        mShowRigidBodies = v;
    }
    bool showRigidBodies() const {
        return mShowRigidBodies;
    }

    // --- VMD (delegates to mAnimCtrl) ---
    int  loadVmd(const std::filesystem::path& p) { return mAnimCtrl.loadVmd(p); }
    void playVmd(int tid, std::function<void(int)> cb = nullptr) { mAnimCtrl.playVmd(tid, std::move(cb)); }
    void pauseVmd(int tid)    { mAnimCtrl.pauseVmd(tid); }
    void stopVmd(int tid)     { mAnimCtrl.stopVmd(tid); }
    void removeVmd(int tid)   { mAnimCtrl.removeVmd(tid); }
    void playAllVmd()         { mAnimCtrl.playAllVmd(); }
    void pauseAllVmd()        { mAnimCtrl.pauseAllVmd(); }
    void stopAllVmd()         { mAnimCtrl.stopAllVmd(); }
    bool isVmdPlaying() const { return mAnimCtrl.isVmdPlaying(); }
    int  vmdTrackCount() const        { return mAnimCtrl.vmdTrackCount(); }
    bool isVmdPlaying(int tid) const  { return mAnimCtrl.isVmdPlaying(tid); }
    float vmdCurrentFrame(int tid) const { return mAnimCtrl.vmdCurrentFrame(tid); }
    float vmdMaxFrame(int tid) const    { return mAnimCtrl.vmdMaxFrame(tid); }
    void  setVmdFrame(int tid, float f) { mAnimCtrl.setVmdFrame(tid, f); }

    // --- VPD (delegates to mAnimCtrl) ---
    int  loadVpd(const std::filesystem::path& p) { return mAnimCtrl.loadVpd(p); }
    void applyVpd(int vpdId)   { mAnimCtrl.applyVpd(vpdId, mPmx, mPoseWorld); applyVpdPost(); }
    void resetPose()           { mAnimCtrl.resetPose(mPmx, mPoseWorld); resetPosePost(); }
    void syncVpdPose()         { mAnimCtrl.syncVpdPose(mPmx, mPoseWorld); resetPosePost(); }
    bool vpdApplied() const    { return mAnimCtrl.vpdApplied(); }
    void removeVpd(int vpdId)  { mAnimCtrl.removeVpd(vpdId); }

    // --- Display toggles ---
    void showModel(bool v)  { mRenderer.showModel = v; }
    void showOutline(bool v){ mRenderer.showOutline = v; }
    void showToon(bool v)   { mRenderer.showToon = v; }
    bool showModel() const  { return mRenderer.showModel; }
    bool showOutline() const{ return mRenderer.showOutline; }
    bool showToon() const   { return mRenderer.showToon; }

    // --- LookAt (delegates to mLookAtCtrl) ---
    void lookAt(int sx, int sy, int sw, int sh) { mLookAtCtrl.start(sx, sy, sw, sh); }
    void resetLookAt() { mLookAtCtrl.reset(); }

    // --- Morphs ---
    void setMorphWeight(const std::string& name, float weight) {
        MMD_INFO("MORPH", "%s = %.2f", name.c_str(), weight);
        mAnimCtrl.setMorphWeight(name, weight);
        mMorphCtl.setMorphWeight(name, weight);
    }
    float savedMorphWeight(const std::string& name) const { return mAnimCtrl.savedMorphWeight(name); }
    void clearMorphs()                                { mAnimCtrl.clearMorphs(); mMorphCtl.clearMorphs(); }
    void setMorphWeights(const std::unordered_map<std::string, float>& w) {
        mAnimCtrl.setMorphWeights(w);
        mMorphCtl.setMorphWeights(w);
    }
    void setIdleBlink(bool on) { mAnimCtrl.setIdleBlink(on); }
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
    PhysicsWorld* physicsWorld() { return &mPhysics; }
    RigidBodyRenderer* physicsDebug();

private:
    void syncBoneTexture();
    void syncMorphOffsets();
    void applyVpdPost();
    void resetPosePost();

    PmxModel mPmx;
    ModelRenderer mRenderer;
    PhysicsWorld mPhysics;
    MorphController mMorphCtl;
    std::unique_ptr<RigidBodyRenderer> mPhysicsDebug;
    AnimationController mAnimCtrl;

    std::vector<std::array<float, 16>> mPoseWorld;
    std::vector<std::array<float, 16>> mBindPoseWorld;
    std::vector<std::array<float, 16>> mInvBindPoseWorld;

    bool mShowRigidBodies = false;
    bool mBoneDirty = true;

    LookAtController mLookAtCtrl;
};

}  // namespace mmd
