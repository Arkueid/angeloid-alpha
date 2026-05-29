#pragma once

#include <array>
#include <filesystem>
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
    std::array<float, 3> lerpVec3(const std::array<float, 3>& a, const std::array<float, 3>& b, float t);
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

// --- Player that advances frames and samples transforms ---

class VmdPlayer {
public:
    explicit VmdPlayer(VmdAnimation anim, float fps = 30.0f);

    void play();
    void pause();
    void stop();

    void update(float deltaTime);

    // Returns (position vec3, rotation quat) or nullptr-equivalent via bool
    bool getBoneTransform(const std::string& boneName,
                          std::array<float, 3>& posOut,
                          std::array<float, 4>& rotOut) const;

    float getMorphWeight(const std::string& morphName) const;

    const VmdAnimation& animation() const { return mAnimation; }
    float currentFrame() const { return mCurrentFrame; }
    bool playing() const { return mPlaying; }

    bool loop() const { return mLoop; }
    void setLoop(bool loop) { mLoop = loop; }
    void setFrame(float frame);
    void setFps(float fps) { mFps = fps; }

private:
    VmdAnimation mAnimation;
    float mFps = 30;
    float mCurrentFrame = 0;
    bool mPlaying = false;
    bool mLoop = true;
};

// --- Mixer: blends multiple VMD layers ---

class VmdMixer {
public:
    explicit VmdMixer(float fps = 30.0f);

    void addVmd(VmdAnimation anim);
    void clear();

    void play();
    void pause();
    void stop();

    void update(float deltaTime);

    bool getBoneTransform(const std::string& boneName,
                          std::array<float, 3>& posOut,
                          std::array<float, 4>& rotOut) const;

    float getMorphWeight(const std::string& morphName) const;

    float currentFrame() const { return mPlayers.empty() ? 0 : mPlayers[0].currentFrame(); }
    float maxFrame() const { return mMaxFrame; }
    bool playing() const { return mPlaying; }
    bool loop() const { return mLoop; }
    void setLoop(bool loop);
    void setFrame(float frame);

private:
    std::vector<VmdPlayer> mPlayers;
    float mFps = 30;
    float mMaxFrame = 0;
    bool mPlaying = false;
    bool mLoop = true;
};
