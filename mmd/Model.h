#pragma once

#include "Camera.h"
#include "anim/BoneSkinning.h"
#include "anim/MorphController.h"
#include "anim/PhysicsWorld.h"
#include "anim/VmdPlayer.h"
#include "anim/VpdLoader.h"
#include "pmx/PmxModel.h"
#include "render/opengl/ModelRenderer.h"
#include "render/opengl/ShaderManager.h"
#include "render/opengl/debug/RigidBodyRenderer.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmd {

class Model {
   public:
    Model() = default;

    // --- Loading ---
    void load(const std::filesystem::path& pmxPath, const std::filesystem::path& texDir,
              const std::filesystem::path& toonDir, const std::filesystem::path& shaderDir);
    void loadVpd(const std::filesystem::path& vpdPath);

    // --- Per-frame ---
    void update(float dt);
    void draw(int screenWidth, int screenHeight);

    // --- Physics ---
    void enablePhysics(bool on);
    bool physicsEnabled() const
    {
        return mPhysics.enabled;
    }
    void showRigidBodies(bool v)
    {
        mShowRigidBodies = v;
    }

    // --- VMD animation ---
    void loadVmd(const std::filesystem::path& path);
    bool hasVmd() const;
    void vmdPlay();
    void vmdPause();
    bool vmdPlaying() const;
    bool vmdLoop() const;
    void setVmdLoop(bool loop);
    float vmdCurrentFrame() const;
    float vmdMaxFrame() const;
    void setVmdFrame(float frame);

    // --- VPD pose ---
    void applyVpd(bool on);
    bool vpdApplied() const
    {
        return mVpdApplied;
    }

    // --- Display toggles ---
    void showModel(bool v)
    {
        mRenderer.showModel = v;
    }
    void showOutline(bool v)
    {
        mRenderer.showOutline = v;
    }
    void showToon(bool v)
    {
        mRenderer.showToon = v;
    }
    bool showModel() const
    {
        return mRenderer.showModel;
    }
    bool showOutline() const
    {
        return mRenderer.showOutline;
    }
    bool showToon() const
    {
        return mRenderer.showToon;
    }
    bool isSkinned() const
    {
        return mRenderer.useSkinning;
    }
    void setSkinning(bool on)
    {
        mRenderer.useSkinning = on;
    }

    // --- Morphs ---
    void setMorphWeight(const std::string& name, float weight);
    void clearMorphs();
    void setMorphWeights(const std::unordered_map<std::string, float>& weights);
    void setIdleBlink(bool on)
    {
        mIdleEnabled = on;
    }
    int morphCount() const;
    std::vector<int> interactableMorphs() const;
    std::string morphName(int index) const;

    // --- Accessors ---
    const std::string& modelName() const
    {
        return mData.name;
    }
    const PmxModel& data() const
    {
        return mData;
    }
    float modelScale() const
    {
        return mRenderer.modelScale();
    }
    const float* modelMatrix() const
    {
        return mRenderer.modelMatrix();
    }

   private:
    void syncBoneTexture();
    void syncMorphOffsets();

    PmxModel mData;
    ModelRenderer mRenderer;
    PhysicsWorld mPhysics;
    MorphController mMorphCtl;
    std::unique_ptr<ShaderManager> mShaders;
    std::unique_ptr<RigidBodyRenderer> mPhysicsDebug;
    std::unique_ptr<VmdMixer> mVmdMixer;

    std::unordered_map<std::string, VpdPose> mVpdPoses;
    bool mVpdApplied = false;

    std::vector<std::array<float, 16>> mPoseWorld;
    std::vector<std::array<float, 16>> mBindPoseWorld;
    std::filesystem::path mVpdPath;

    bool mIdleEnabled = true;
    float mIdleTime = 0;
    bool mShowRigidBodies = false;
};

}  // namespace mmd
