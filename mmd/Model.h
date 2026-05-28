#pragma once

#include "pmx/PmxModel.h"
#include "render/opengl/ModelRenderer.h"
#include "anim/PhysicsWorld.h"
#include "anim/MorphController.h"
#include "anim/BoneSkinning.h"
#include "anim/VpdLoader.h"
#include "anim/VmdPlayer.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "render/opengl/debug/RigidBodyRenderer.h"

class ShaderManager;

namespace mmd {

class Model {
public:
    Model() = default;

    // --- Loading ---
    void load(const std::filesystem::path& pmxPath,
              const std::filesystem::path& texDir,
              const std::filesystem::path& toonDir);
    void loadVpd(const std::filesystem::path& vpdPath);

    // --- Per-frame ---
    void update(float dt);
    void draw(ShaderManager& shaders,
              const std::array<float, 16>& proj, const std::array<float, 16>& view,
              const float* cameraPos);
    void drawPhysicsDebug(ShaderManager& shaders,
                          const std::array<float, 16>& proj, const std::array<float, 16>& view);

    // --- Physics ---
    void enablePhysics(bool on);
    bool physicsEnabled() const { return mPhysics.enabled; }

    // --- VMD animation ---
    void setVmd(std::unique_ptr<VmdMixer> mixer);
    VmdMixer* vmdMixer() const { return mVmdMixer.get(); }

    // --- VPD pose ---
    void applyVpd(bool on);
    bool vpdApplied() const { return mVpdApplied; }

    // --- Display toggles ---
    void showModel(bool v) { mRenderer.showModel = v; }
    void showOutline(bool v) { mRenderer.showOutline = v; }
    void showToon(bool v) { mRenderer.showToon = v; }
    bool isSkinned() const { return mRenderer.useSkinning; }

    // --- Morphs ---
    void setMorphWeight(const std::string& name, float weight);
    void clearMorphs();
    void setMorphWeights(const std::unordered_map<std::string, float>& weights);
    MorphController& morphController() { return mMorphCtl; }
    void setIdleBlink(bool on) { mIdleEnabled = on; }

    // --- Accessors ---
    const PmxModel& data() const { return mData; }
    ModelRenderer& renderer() { return mRenderer; }
    PhysicsWorld& physics() { return mPhysics; }
    void showPhysicsDebug(bool v) { mShowPhysicsDebug = v; }
    float modelScale() const { return mRenderer.modelScale(); }
    const float* modelMatrix() const { return mRenderer.modelMatrix(); }

private:
    void syncBoneTexture();
    void syncMorphOffsets();

    PmxModel mData;
    ModelRenderer mRenderer;
    PhysicsWorld mPhysics;
    MorphController mMorphCtl;
    std::unique_ptr<RigidBodyRenderer> mPhysicsDebug;
    std::unique_ptr<VmdMixer> mVmdMixer;

    std::unordered_map<std::string, VpdPose> mVpdPoses;
    bool mVpdApplied = false;

    std::vector<std::array<float, 16>> mPoseWorld;
    std::vector<std::array<float, 16>> mBindPoseWorld;
    std::filesystem::path mVpdPath;

    bool mIdleEnabled = true;
    float mIdleTime = 0;
    bool mShowPhysicsDebug = false;
};

} // namespace mmd
