#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// --- Keyframe types ---

struct VmdBoneKeyframe {
    std::string boneName;
    int frame = 0;
    float px = 0, py = 0, pz = 0;
    float qx = 0, qy = 0, qz = 0, qw = 1;
    uint8_t interpolation[64] = {};
};

struct VmdMorphKeyframe {
    std::string morphName;
    int frame = 0;
    float weight = 0;
};

// --- Interpolation helpers ---

namespace VmdInterp {
float bezier(float t, float p0, float p1, float p2, float p3);
float interpBezier(float t, const uint8_t* interp, int axis);
float lerp(float a, float b, float t);
std::array<float, 3> lerpVec3(const std::array<float, 3>& a, const std::array<float, 3>& b,
                              float t);
}

// --- Single VMD animation ---

class VmdAnimation {
public:
    std::string modelName;
    std::unordered_map<std::string, std::vector<VmdBoneKeyframe>> boneKeyframes;
    std::unordered_map<std::string, std::vector<VmdMorphKeyframe>> morphKeyframes;

    int maxFrame = 0;

    static VmdAnimation load(const std::filesystem::path& path);
};

// --- VmdPlayState: per-track playback state referencing shared animation data ---

class VmdPlayState {
public:
    explicit VmdPlayState(const VmdAnimation* anim = nullptr, float fps = 30.0f);

    void play(std::function<void(int)> onEnd = nullptr);
    void pause();
    void stop();

    bool update(float deltaTime);

    bool getBoneTransform(const std::string& boneName, std::array<float, 3>& posOut,
                          std::array<float, 4>& rotOut) const;

    float getMorphWeight(const std::string& morphName) const;

    const VmdAnimation* animation() const {
        return mAnimation;
    }
    float currentFrame() const {
        return mCurrentFrame;
    }
    bool playing() const {
        return mPlaying;
    }

    void setFrame(float frame);
    void setFps(float fps) {
        mFps = fps;
    }

private:
    const VmdAnimation* mAnimation = nullptr;
    float mFps = 30;
    float mCurrentFrame = 0;
    bool mPlaying = false;
    bool mLoop = false;
    std::function<void(int)> mOnEnd;
    int mTrackId = -1;
    friend class VmdMixer;
};

// Backward-compatible alias
using VmdPlayer = VmdPlayState;

// --- Mixer: blends multiple VMD layers ---

class VmdMixer {
public:
    explicit VmdMixer(float fps = 30.0f);

    int  addVmd(const VmdAnimation* anim);
    void removeVmd(int trackId);
    void clear();

    void play(int trackId, std::function<void(int)> onEnd = nullptr);
    void pause(int trackId);
    void stop(int trackId);

    void playAll();
    void pauseAll();
    void stopAll();

    bool update(float deltaTime);

    bool getBoneTransform(const std::string& boneName, std::array<float, 3>& posOut,
                          std::array<float, 4>& rotOut) const;

    float getMorphWeight(const std::string& morphName) const;

    float currentFrame(int trackId) const;
    bool  playing(int trackId) const;
    int   trackCount() const {
        return (int)mPlayStates.size();
    }

    // global: first track
    float currentFrame() const {
        return mPlayStates.empty() ? 0 : mPlayStates[0].currentFrame();
    }
    bool playing() const;

    void setFrame(int trackId, float frame);

private:
    std::vector<VmdPlayState> mPlayStates;
    float mFps = 30;
    int mNextId = 1;
};
