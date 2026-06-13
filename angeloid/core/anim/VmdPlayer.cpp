#include "core/anim/VmdPlayer.h"

#include "core/encoding/Encoding.h"
#include "core/util/Log.h"
#include "core/math/VecMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

static std::string decodeShiftJisName(const char* raw, int maxLen) {
    std::string bytes;
    for (int i = 0; i < maxLen && raw[i] != '\0'; ++i)
        bytes += raw[i];
    return Encoding::cp932ToUtf8(bytes);
}

// --- VMD Binary Format Loader ---
//
// VMD (Vocaloid Motion Data) format:
//   Header:  30-byte magic ("Vocaloid Motion Data 0002") + 20-byte model name (Shift-JIS)
//   Bones:   uint32 count, then per-bone: 15-byte name + frame(int32) + pos(vec3) + rot(quat) + 64-byte bezier curves
//   Morphs:  uint32 count, then per-morph: 15-byte name + frame(int32) + weight(float32)
// All text is Shift-JIS encoded; bone/morph names are fixed 15-byte fields.

VmdAnimation VmdAnimation::load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        MMD_ERROR("VMD", "Failed to open VMD: %s", path.string().c_str());
        return {};
    }

    VmdAnimation anim;

    char magic[31] = {};
    file.read(magic, 30);
    std::string magicStr(magic);
    if (magicStr.find("Vocaloid Motion Data") == std::string::npos) {
        MMD_ERROR("VMD", "Invalid VMD magic: %s", path.string().c_str());
        return {};
    }

    // Model name (20 bytes, Shift-JIS)
    char modelName[21] = {};
    file.read(modelName, 20);
    anim.modelName = decodeShiftJisName(modelName, 20);

    // Bone keyframes
    uint32_t boneCount = 0;
    file.read((char*)&boneCount, 4);
    anim.boneKeyframes.reserve(boneCount);

    for (uint32_t i = 0; i < boneCount; ++i) {
        VmdBoneKeyframe kf;
        char name[16] = {};
        file.read(name, 15);
        kf.boneName = decodeShiftJisName(name, 15);

        file.read((char*)&kf.frame, 4);
        file.read((char*)&kf.px, 4);
        file.read((char*)&kf.py, 4);
        file.read((char*)&kf.pz, 4);
        file.read((char*)&kf.qx, 4);
        file.read((char*)&kf.qy, 4);
        file.read((char*)&kf.qz, 4);
        file.read((char*)&kf.qw, 4);
        file.read((char*)kf.interpolation, 64);

        anim.boneKeyframes[kf.boneName].push_back(std::move(kf));
        anim.maxFrame = std::max(anim.maxFrame, kf.frame);
    }

    // Morph keyframes
    uint32_t morphCount = 0;
    file.read((char*)&morphCount, 4);

    for (uint32_t i = 0; i < morphCount; ++i) {
        VmdMorphKeyframe kf;
        char name[16] = {};
        file.read(name, 15);
        kf.morphName = decodeShiftJisName(name, 15);

        file.read((char*)&kf.frame, 4);
        file.read((char*)&kf.weight, 4);

        anim.morphKeyframes[kf.morphName].push_back(std::move(kf));
        anim.maxFrame = std::max(anim.maxFrame, kf.frame);
    }

    // Sort keyframes by frame
    for (auto& [_, kfs] : anim.boneKeyframes) {
        std::sort(kfs.begin(), kfs.end(), [](auto& a, auto& b) {
            return a.frame < b.frame;
        });
    }
    for (auto& [_, kfs] : anim.morphKeyframes) {
        std::sort(kfs.begin(), kfs.end(), [](auto& a, auto& b) {
            return a.frame < b.frame;
        });
    }

    return anim;
}

// --- Bezier interpolation (VMD curve system) ---
//
// VMD stores per-axis interpolation curves as 4 control points (x1,y1,x2,y2,x3,y3,x4,y4)
// encoded as uint8 values (0-127 → 0.0-1.0). The curve maps input time t to output
// parameter u: given a linear t between keyframes, compute the Bezier x-coordinate at t,
// then find the corresponding y which is the warped interpolation parameter.
//
// The system: x = bezier(t, 0, ax, bx, 1) — maps t→x
//             y = bezier(x, 0, ay, by, 1) — maps x→y (the actual warp)
// Then y is used as the lerp factor for the animated value.

float VmdInterp::bezier(float t, float p0, float p1, float p2, float p3) {
    float u = 1.0f - t;
    return u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t * p3;
}

// Given an x-coordinate on a Bezier curve (0,ax,bx,1), binary-search for the t that produces it.
// This inverts x = bezier(t, 0, ax, bx, 1) → t.
static float solveBezierX(float targetX, float ax, float bx, int iterations = 16) {
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < iterations; ++i) {
        float mid = (lo + hi) * 0.5f;
        float x = VmdInterp::bezier(mid, 0.0f, ax, bx, 1.0f);
        if (x < targetX)
            lo = mid;
        else
            hi = mid;
    }
    return (lo + hi) * 0.5f;
}

float VmdInterp::interpBezier(float t, const uint8_t* interp, int axis) {
    // Each axis has 16 bytes: [x1, y1] x 4 points, stored as uint8 / 127
    int idx = axis * 16;
    float ax = interp[idx + 0] / 127.0f;
    float ay = interp[idx + 4] / 127.0f;
    float bx = interp[idx + 8] / 127.0f;
    float by = interp[idx + 12] / 127.0f;

    if (std::abs(ax - ay) < 0.001f && std::abs(bx - by) < 0.001f)
        return t;

    float x = solveBezierX(t, ax, bx);
    return bezier(x, 0.0f, ay, by, 1.0f);
}

float VmdInterp::lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

std::array<float, 3> VmdInterp::lerpVec3(const std::array<float, 3>& a,
                                         const std::array<float, 3>& b, float t) {
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}

// --- VmdPlayState ---

VmdPlayState::VmdPlayState(const VmdAnimation* anim, float fps)
    : mAnimation(anim), mFps(fps), mPlaying(anim != nullptr) {
}

void VmdPlayState::play(std::function<void(int)> onEnd) {
    mPlaying = true;
    mOnEnd = std::move(onEnd);
}
void VmdPlayState::pause() {
    mPlaying = false;
}
void VmdPlayState::stop() {
    mPlaying = false;
    mCurrentFrame = 0;
}
void VmdPlayState::setFrame(float f) {
    if (mAnimation)
        mCurrentFrame = std::max(0.0f, std::min(f, (float)mAnimation->maxFrame));
}

bool VmdPlayState::update(float deltaTime) {
    if (!mPlaying || !mAnimation || mAnimation->maxFrame <= 0)
        return false;
    mCurrentFrame += deltaTime * mFps;

    if (mCurrentFrame >= (float)mAnimation->maxFrame) {
        if (mLoop) {
            mCurrentFrame = 0;
        }
        else {
            mCurrentFrame = (float)mAnimation->maxFrame;
            mPlaying = false;
            if (mOnEnd)
                mOnEnd(mTrackId);
            mOnEnd = nullptr;
        }
    }
    return mPlaying;
}

bool VmdPlayState::getBoneTransform(const std::string& boneName, std::array<float, 3>& posOut,
                                    std::array<float, 4>& rotOut) const {
    if (!mAnimation)
        return false;
    auto it = mAnimation->boneKeyframes.find(boneName);
    if (it == mAnimation->boneKeyframes.end())
        return false;

    const auto& kfs = it->second;
    if (kfs.empty())
        return false;

    if (kfs.size() == 1) {
        posOut = {kfs[0].px, kfs[0].py, kfs[0].pz};
        rotOut = {kfs[0].qx, kfs[0].qy, kfs[0].qz, kfs[0].qw};
        return true;
    }

    float frame = mCurrentFrame;
    int iframe = (int)frame;
    if (iframe <= kfs.front().frame) {
        posOut = {kfs.front().px, kfs.front().py, kfs.front().pz};
        rotOut = {kfs.front().qx, kfs.front().qy, kfs.front().qz, kfs.front().qw};
        return true;
    }
    if (iframe >= kfs.back().frame) {
        posOut = {kfs.back().px, kfs.back().py, kfs.back().pz};
        rotOut = {kfs.back().qx, kfs.back().qy, kfs.back().qz, kfs.back().qw};
        return true;
    }

    auto kfIt = std::lower_bound(kfs.begin(), kfs.end(), iframe,
        [](const VmdBoneKeyframe& kf, int f) { return kf.frame < f; });
    int right = (int)(kfIt - kfs.begin());
    if (right == 0) {
        posOut = {kfs[0].px, kfs[0].py, kfs[0].pz};
        rotOut = {kfs[0].qx, kfs[0].qy, kfs[0].qz, kfs[0].qw};
        return true;
    }
    int left = right - 1;

    const auto& kl = kfs[left];
    const auto& kr = kfs[right];
    float diff = (float)(kr.frame - kl.frame);
    if (diff == 0) {
        posOut = {kl.px, kl.py, kl.pz};
        rotOut = {kl.qx, kl.qy, kl.qz, kl.qw};
        return true;
    }

    float t = (frame - kl.frame) / diff;

    float tx = VmdInterp::interpBezier(t, kl.interpolation, 0);
    float ty = VmdInterp::interpBezier(t, kl.interpolation, 1);
    float tz = VmdInterp::interpBezier(t, kl.interpolation, 2);
    float tr = VmdInterp::interpBezier(t, kl.interpolation, 3);

    posOut[0] = VmdInterp::lerp(kl.px, kr.px, tx);
    posOut[1] = VmdInterp::lerp(kl.py, kr.py, ty);
    posOut[2] = VmdInterp::lerp(kl.pz, kr.pz, tz);

    std::array<float, 4> ql = {kl.qx, kl.qy, kl.qz, kl.qw};
    std::array<float, 4> qr = {kr.qx, kr.qy, kr.qz, kr.qw};
    rotOut = quatSlerp(ql, qr, tr);
    return true;
}

float VmdPlayState::getMorphWeight(const std::string& morphName) const {
    if (!mAnimation)
        return 0;
    auto it = mAnimation->morphKeyframes.find(morphName);
    if (it == mAnimation->morphKeyframes.end())
        return 0;

    const auto& kfs = it->second;
    if (kfs.empty())
        return 0;
    if (kfs.size() == 1)
        return kfs[0].weight;

    float frame = mCurrentFrame;
    int iframe = (int)frame;
    if (iframe <= kfs.front().frame)
        return kfs.front().weight;
    if (iframe >= kfs.back().frame)
        return kfs.back().weight;

    auto kfIt = std::lower_bound(kfs.begin(), kfs.end(), iframe,
        [](const VmdMorphKeyframe& kf, int f) { return kf.frame < f; });
    int right = (int)(kfIt - kfs.begin());
    if (right == 0)
        return kfs[0].weight;
    int left = right - 1;
    float diff = (float)(kfs[right].frame - kfs[left].frame);
    if (diff == 0)
        return kfs[left].weight;
    float t = (frame - kfs[left].frame) / diff;
    return VmdInterp::lerp(kfs[left].weight, kfs[right].weight, t);
}

// --- VmdMixer ---

VmdMixer::VmdMixer(float fps) : mFps(fps) {
}

int VmdMixer::addVmd(const VmdAnimation* anim) {
    if (!anim)
        return -1;
    int id = mNextId++;
    mPlayStates.emplace_back(anim, mFps);
    mPlayStates.back().mTrackId = id;
    return id;
}

void VmdMixer::removeVmd(int trackId) {
    mPlayStates.erase(
        std::remove_if(mPlayStates.begin(), mPlayStates.end(),
                       [trackId](const VmdPlayState& p) { return p.mTrackId == trackId; }),
        mPlayStates.end());
}

void VmdMixer::clear() {
    mPlayStates.clear();
}

void VmdMixer::play(int trackId, std::function<void(int)> onEnd) {
    for (auto& p : mPlayStates)
        if (p.mTrackId == trackId) {
            p.play(std::move(onEnd));
            return;
        }
}

void VmdMixer::pause(int trackId) {
    for (auto& p : mPlayStates)
        if (p.mTrackId == trackId) {
            p.pause();
            return;
        }
}

void VmdMixer::stop(int trackId) {
    for (auto& p : mPlayStates)
        if (p.mTrackId == trackId) {
            p.stop();
            return;
        }
}

void VmdMixer::playAll() {
    for (auto& p : mPlayStates)
        p.play();
}
void VmdMixer::pauseAll() {
    for (auto& p : mPlayStates)
        p.pause();
}
void VmdMixer::stopAll() {
    for (auto& p : mPlayStates)
        p.stop();
}

bool VmdMixer::update(float deltaTime) {
    bool updated = false;
    for (auto& p : mPlayStates) {
        updated |= p.update(deltaTime);
    }
    return updated;
}

float VmdMixer::currentFrame(int trackId) const {
    for (auto& p : mPlayStates)
        if (p.mTrackId == trackId)
            return p.currentFrame();
    return 0;
}

bool VmdMixer::playing(int trackId) const {
    for (auto& p : mPlayStates)
        if (p.mTrackId == trackId)
            return p.playing();
    return false;
}

bool VmdMixer::playing() const {
    for (const auto& p : mPlayStates)
        if (p.playing())
            return true;
    return false;
}

void VmdMixer::setFrame(int trackId, float frame) {
    for (auto& p : mPlayStates)
        if (p.mTrackId == trackId) {
            p.setFrame(frame);
            return;
        }
}

// Blend multiple VMD layers: positions are summed (additive layering),
// rotations are blended via quaternion slerp at 0.5 weight per layer.
// This means the first layer's rotation dominates, and each subsequent
// layer blends 50/50 with the accumulated result — a simple but effective
// multi-track motion layering scheme.
bool VmdMixer::getBoneTransform(const std::string& boneName, std::array<float, 3>& posOut,
                                std::array<float, 4>& rotOut) const {
    bool has = false;
    std::array<float, 3> pos = {0, 0, 0};
    std::array<float, 4> rot = {0, 0, 0, 1};

    for (const auto& p : mPlayStates) {
        std::array<float, 3> pp;
        std::array<float, 4> pr;
        if (p.getBoneTransform(boneName, pp, pr)) {
            pos[0] += pp[0];
            pos[1] += pp[1];
            pos[2] += pp[2];
            if (!has)
                rot = pr;
            else
                rot = quatSlerp(rot, pr, 0.5f);
            has = true;
        }
    }
    if (!has)
        return false;
    posOut = pos;
    rotOut = rot;
    return true;
}

float VmdMixer::getMorphWeight(const std::string& morphName) const {
    float sum = 0;
    bool has = false;
    for (const auto& p : mPlayStates) {
        float w = p.getMorphWeight(morphName);
        if (w != 0) {
            sum += w;
            has = true;
        }
    }
    return has ? std::max(0.0f, std::min(1.0f, sum)) : 0;
}

