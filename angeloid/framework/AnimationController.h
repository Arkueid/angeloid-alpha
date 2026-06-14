#pragma once

#include "core/anim/VmdPlayer.h"
#include "core/anim/VpdLoader.h"
#include "core/pmx/PmxModel.h"

#include <array>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmd {

class AnimationController {
public:
    AnimationController() = default;

    void init();

    // --- Per-frame ---
    struct UpdateResult {
        bool bonesChanged = false;
        std::unordered_map<std::string, float> vmdMorphWeights;
        const VpdPoseMap* activeVpd = nullptr;
    };
    UpdateResult update(float dt,
                        const PmxModel& pmx,
                        std::vector<std::array<float, 16>>& outPoseWorld);

    // --- VMD playback ---
    int  loadVmd(const std::filesystem::path& path);
    void playVmd(int trackId, std::function<void(int)> onEnd = nullptr);
    void pauseVmd(int trackId);
    void stopVmd(int trackId);
    void removeVmd(int trackId);
    void playAllVmd();
    void pauseAllVmd();
    void stopAllVmd();
    bool isVmdPlaying() const;
    bool isVmdPlaying(int trackId) const;
    int  vmdTrackCount() const;
    float vmdCurrentFrame(int trackId) const;
    float vmdMaxFrame(int trackId) const;
    void  setVmdFrame(int trackId, float frame);

    // --- VPD pose ---
    int  loadVpd(const std::filesystem::path& path);
    void applyVpd(int vpdId,
                  const PmxModel& pmx,
                  std::vector<std::array<float, 16>>& outPoseWorld);
    void resetPose(const PmxModel& pmx,
                   std::vector<std::array<float, 16>>& outPoseWorld);
    void syncVpdPose(const PmxModel& pmx,
                     std::vector<std::array<float, 16>>& outPoseWorld);
    bool vpdApplied() const { return mActiveVpdId >= 0; }
    void removeVpd(int vpdId);

    // --- User morph weights ---
    void setMorphWeight(const std::string& name, float weight);
    float savedMorphWeight(const std::string& name) const;
    void setMorphWeights(const std::unordered_map<std::string, float>& weights);
    void clearMorphs();

    // --- Idle ---
    void setIdleBlink(bool on) { mIdleEnabled = on; }
    bool idleEnabled() const { return mIdleEnabled; }
    float idleTime() const { return mIdleTime; }

    // --- Bind pose reference ---
    void setBindPose(const std::vector<std::array<float, 16>>& bindPose) {
        mBindPoseWorld = &bindPose;
    }

private:
    void computePoseWorld(const PmxModel& pmx,
                          std::vector<std::array<float, 16>>& outPoseWorld);

    std::unique_ptr<VmdMixer> mVmdMixer;
    std::unordered_map<std::string, std::pair<std::array<float, 3>, std::array<float, 4>>> mVmdBoneCache;
    std::deque<VmdAnimation> mVmdAnimations;

    std::vector<std::pair<int, VpdPoseMap>> mVpdPoses;
    int mActiveVpdId = -1;
    int mVpdNextId = 1;

    bool mIdleEnabled = true;
    float mIdleTime = 0;
    bool mClearVmd = false;

    std::unordered_map<std::string, float> mSavedWeights;

    const std::vector<std::array<float, 16>>* mBindPoseWorld = nullptr;
};

} // namespace mmd
