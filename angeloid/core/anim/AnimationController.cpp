#include "core/anim/AnimationController.h"

#include "core/anim/BoneSkinning.h"
#include "core/anim/VmdPlayer.h"
#include "core/anim/VpdLoader.h"
#include "core/util/Log.h"

#include <cmath>

namespace mmd {

void AnimationController::init() {
    mVmdMixer = std::make_unique<VmdMixer>();
}

AnimationController::UpdateResult AnimationController::update(
    float dt,
    const PmxModel& pmx,
    std::vector<std::array<float, 16>>& outPoseWorld) {
    UpdateResult result;
    bool vmdUpdated = mVmdMixer->update(dt);
    result.bonesChanged = vmdUpdated;

    if (vmdUpdated || !mClearVmd) {
        bool needCompute = mVmdMixer->playing() || mFrameSeeked;
        if (needCompute) {
            mFrameSeeked = false;
            mVmdBoneCache.clear();
            for (const auto& bone : pmx.bones) {
                std::array<float, 3> pos;
                std::array<float, 4> rot;
                if (mVmdMixer->getBoneTransform(bone.name, pos, rot))
                    mVmdBoneCache[bone.name] = {pos, rot};
            }
            if (!mVmdBoneCache.empty())
                computePoseWorld(pmx, outPoseWorld);
        }
        // Only collect VMD morphs when playing/seeking; otherwise
        // clearMorphs() would be immediately overwritten next frame
        if (needCompute) {
            for (const auto& m : pmx.morphs) {
                float w = mVmdMixer->getMorphWeight(m.name);
                if (w != 0)
                    result.vmdMorphWeights[m.name] = w;
            }
        }
    }

    // Active VPD pointer for physics recompute
    for (auto& [id, poses] : mVpdPoses) {
        if (id == mActiveVpdId) {
            result.activeVpd = &poses;
            break;
        }
    }

    // Idle time
    if (mIdleEnabled && !mVmdMixer->playing()) {
        mIdleTime += dt;
    }

    return result;
}

void AnimationController::computePoseWorld(
    const PmxModel& pmx,
    std::vector<std::array<float, 16>>& outPoseWorld) {
    if (mActiveVpdId >= 0) {
        for (auto& [id, poses] : mVpdPoses) {
            if (id == mActiveVpdId) {
                outPoseWorld = BoneSkinning::computePoseWorldMatrices(pmx, poses, mVmdBoneCache);
                return;
            }
        }
    }
    outPoseWorld = BoneSkinning::computePoseWorldMatrices(pmx, {}, mVmdBoneCache);
}

// --- VMD ---

int AnimationController::loadVmd(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return -1;
    auto anim = VmdAnimation::load(path);
    std::string name = anim.modelName;
    int maxF = anim.maxFrame;
    mVmdAnimations.push_back(std::move(anim));
    int id = mVmdMixer->addVmd(&mVmdAnimations.back());
    MMD_INFO("MODEL", "VMD loaded [id=%d]: %s (maxFrame=%d) from %s", id, name.c_str(), maxF,
             reinterpret_cast<const char*>(path.filename().u8string().c_str()));
    return id;
}

void AnimationController::playVmd(int trackId, std::function<void(int)> onEnd) {
    MMD_INFO("MODEL", "VMD play [id=%d]%s", trackId, onEnd ? " with callback" : "");
    mClearVmd = false;
    mVmdMixer->play(trackId, std::move(onEnd));
}
void AnimationController::pauseVmd(int trackId) {
    MMD_INFO("MODEL", "VMD pause [id=%d]", trackId);
    mVmdMixer->pause(trackId);
}
void AnimationController::stopVmd(int trackId) {
    MMD_INFO("MODEL", "VMD stop [id=%d]", trackId);
    mVmdMixer->stop(trackId);
}
void AnimationController::removeVmd(int trackId) {
    MMD_INFO("MODEL", "VMD remove [id=%d]", trackId);
    mVmdMixer->removeVmd(trackId);
}

void AnimationController::playAllVmd() {
    MMD_INFO("MODEL", "VMD play all");
    mClearVmd = false;
    mVmdMixer->playAll();
}
void AnimationController::pauseAllVmd() {
    MMD_INFO("MODEL", "VMD pause all");
    mVmdMixer->pauseAll();
}
void AnimationController::stopAllVmd() {
    MMD_INFO("MODEL", "VMD stop all");
    mVmdMixer->stopAll();
}
bool AnimationController::isVmdPlaying() const {
    return mVmdMixer->playing();
}

int AnimationController::vmdTrackCount() const {
    return mVmdMixer ? mVmdMixer->trackCount() : 0;
}
bool AnimationController::isVmdPlaying(int trackId) const {
    return mVmdMixer && mVmdMixer->playing(trackId);
}
float AnimationController::vmdCurrentFrame(int trackId) const {
    return mVmdMixer ? mVmdMixer->currentFrame(trackId) : 0;
}
float AnimationController::vmdMaxFrame(int trackId) const {
    return mVmdMixer ? mVmdMixer->maxFrame(trackId) : 0;
}

void AnimationController::setVmdFrame(int trackId, float frame) {
    mVmdMixer->setFrame(trackId, frame);
    mFrameSeeked = true;
}

// --- VPD ---

int AnimationController::loadVpd(const std::filesystem::path& vpdPath) {
    if (!std::filesystem::exists(vpdPath))
        return -1;
    auto poses = VpdLoader::load(vpdPath);
    int id = mVpdNextId++;
    MMD_INFO("MODEL", "VPD loaded [id=%d]: %zu poses (%s)", id, poses.size(),
             reinterpret_cast<const char*>(vpdPath.filename().u8string().c_str()));
    mVpdPoses.push_back({id, std::move(poses)});
    return id;
}

void AnimationController::applyVpd(int vpdId,
                                   const PmxModel& pmx,
                                   std::vector<std::array<float, 16>>& outPoseWorld) {
    MMD_INFO("MODEL", "VPD apply [id=%d]", vpdId);
    for (auto& [id, poses] : mVpdPoses) {
        if (id == vpdId) {
            mActiveVpdId = vpdId;
            outPoseWorld = BoneSkinning::computePoseWorldMatrices(pmx, poses);
            return;
        }
    }
}

void AnimationController::resetPose(const PmxModel& pmx,
                                    std::vector<std::array<float, 16>>& outPoseWorld) {
    if (mVmdMixer->playing()) {
        MMD_INFO("MODEL", "Cannot reset pose while VMD is playing");
        return;
    }
    mClearVmd = true;
    MMD_INFO("MODEL", "Pose reset to bind pose");
    mActiveVpdId = -1;
    outPoseWorld = BoneSkinning::computePoseWorldMatrices(pmx);
}

void AnimationController::syncVpdPose(const PmxModel& pmx,
                                      std::vector<std::array<float, 16>>& outPoseWorld) {
    if (mVmdMixer->playing()) {
        MMD_INFO("MODEL", "Cannot sync VPD pose while VMD is playing");
        return;
    }
    mClearVmd = true;
    if (mActiveVpdId >= 0) {
        for (auto& [id, poses] : mVpdPoses)
            if (id == mActiveVpdId) {
                MMD_INFO("MODEL", "VPD pose sync [id=%d]", id);
                outPoseWorld = BoneSkinning::computePoseWorldMatrices(pmx, poses);
                return;
            }
    }
    MMD_INFO("MODEL", "VPD pose sync: no active VPD, using bind pose");
    if (mBindPoseWorld)
        outPoseWorld = *mBindPoseWorld;
}

void AnimationController::removeVpd(int vpdId) {
    mVpdPoses.erase(std::remove_if(mVpdPoses.begin(), mVpdPoses.end(),
                                   [vpdId](auto& p) {
                                       return p.first == vpdId;
                                   }),
                    mVpdPoses.end());
    if (mActiveVpdId == vpdId)
        mActiveVpdId = -1;
}

// --- Morph weights ---

void AnimationController::setMorphWeight(const std::string& name, float weight) {
    mSavedWeights[name] = weight;
}

float AnimationController::savedMorphWeight(const std::string& name) const {
    auto it = mSavedWeights.find(name);
    return it != mSavedWeights.end() ? it->second : 0.0f;
}

void AnimationController::setMorphWeights(const std::unordered_map<std::string, float>& weights) {
    for (auto& [name, w] : weights)
        mSavedWeights[name] = w;
}

void AnimationController::clearMorphs() {
    mSavedWeights.clear();
}

} // namespace mmd
