#include "anim/VmdPlayer.h"
#include "core/Encoding.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

static std::string decodeShiftJisName(const char* raw, int maxLen)
{
    std::string bytes;
    for (int i = 0; i < maxLen && raw[i] != '\0'; ++i) bytes += raw[i];
    return Encoding::cp932ToUtf8(bytes);
}

// --- VMD Loader ---

VmdAnimation VmdAnimation::load(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open VMD: " + path.string());
    }

    VmdAnimation anim;

    // Magic (30 bytes): "Vocaloid Motion Data 0002"
    char magic[31] = {};
    file.read(magic, 30);
    std::string magicStr(magic);
    if (magicStr.find("Vocaloid Motion Data") == std::string::npos) {
        throw std::runtime_error("Invalid VMD magic: " + path.string());
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
        std::sort(kfs.begin(), kfs.end(),
                  [](auto& a, auto& b) { return a.frame < b.frame; });
    }
    for (auto& [_, kfs] : anim.morphKeyframes) {
        std::sort(kfs.begin(), kfs.end(),
                  [](auto& a, auto& b) { return a.frame < b.frame; });
    }

    return anim;
}

// --- Bezier interpolation ---

float VmdInterp::bezier(float t, float p0, float p1, float p2, float p3)
{
    float u = 1.0f - t;
    return u*u*u*p0 + 3.0f*u*u*t*p1 + 3.0f*u*t*t*p2 + t*t*t*p3;
}

static float solveBezierX(float targetX, float ax, float bx, int iterations = 16)
{
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < iterations; ++i) {
        float mid = (lo + hi) * 0.5f;
        float x = VmdInterp::bezier(mid, 0.0f, ax, bx, 1.0f);
        if (x < targetX) lo = mid;
        else hi = mid;
    }
    return (lo + hi) * 0.5f;
}

float VmdInterp::interpBezier(float t, const uint8_t* interp, int axis)
{
    // Each axis has 16 bytes: [x1, y1] x 4 points, stored as uint8 / 127
    int idx = axis * 16;
    float ax = interp[idx + 0]  / 127.0f;
    float ay = interp[idx + 4]  / 127.0f;
    float bx = interp[idx + 8]  / 127.0f;
    float by = interp[idx + 12] / 127.0f;

    if (std::abs(ax - ay) < 0.001f && std::abs(bx - by) < 0.001f)
        return t;

    float x = solveBezierX(t, ax, bx);
    return bezier(x, 0.0f, ay, by, 1.0f);
}

float VmdInterp::lerp(float a, float b, float t) { return a + (b - a) * t; }

std::array<float, 3> VmdInterp::lerpVec3(const std::array<float, 3>& a,
                                           const std::array<float, 3>& b, float t)
{
    return {a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t};
}

std::array<float, 4> VmdInterp::slerpQuat(const std::array<float, 4>& qa,
                                           const std::array<float, 4>& qb, float t)
{
    std::array<float, 4> a = qa, b = qb;
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    if (dot < 0) { b[0] = -b[0]; b[1] = -b[1]; b[2] = -b[2]; b[3] = -b[3]; dot = -dot; }

    if (dot > 0.9995f) {
        std::array<float, 4> r = {a[0] + t*(b[0] - a[0]), a[1] + t*(b[1] - a[1]),
                                   a[2] + t*(b[2] - a[2]), a[3] + t*(b[3] - a[3])};
        float len = std::sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2] + r[3]*r[3]);
        if (len > 0) { r[0]/=len; r[1]/=len; r[2]/=len; r[3]/=len; }
        return r;
    }

    float theta0 = std::acos(std::max(-1.0f, std::min(1.0f, dot)));
    float theta = theta0 * t;
    float sinTheta = std::sin(theta);
    float sinTheta0 = std::sin(theta0);

    float s1 = std::cos(theta) - dot * sinTheta / sinTheta0;
    float s2 = sinTheta / sinTheta0;

    return {s1*a[0] + s2*b[0], s1*a[1] + s2*b[1], s1*a[2] + s2*b[2], s1*a[3] + s2*b[3]};
}

// --- VmdPlayer ---

VmdPlayer::VmdPlayer(VmdAnimation anim, float fps)
    : mAnimation(std::move(anim)), mFps(fps) {}

void VmdPlayer::play()  { mPlaying = true; }
void VmdPlayer::pause() { mPlaying = false; }
void VmdPlayer::stop()  { mPlaying = false; mCurrentFrame = 0; }
void VmdPlayer::setFrame(float f) { mCurrentFrame = std::max(0.0f, std::min(f, (float)mAnimation.maxFrame)); }

void VmdPlayer::update(float deltaTime)
{
    if (!mPlaying || mAnimation.maxFrame <= 0) return;
    mCurrentFrame += deltaTime * mFps;

    if (mCurrentFrame >= (float)mAnimation.maxFrame) {
        if (mLoop) {
            mCurrentFrame = 0;
        } else {
            mCurrentFrame = (float)mAnimation.maxFrame;
            mPlaying = false;
        }
    }
}

bool VmdPlayer::getBoneTransform(const std::string& boneName,
                                  std::array<float, 3>& posOut,
                                  std::array<float, 4>& rotOut) const
{
    auto it = mAnimation.boneKeyframes.find(boneName);
    if (it == mAnimation.boneKeyframes.end()) return false;

    const auto& kfs = it->second;
    if (kfs.empty()) return false;

    if (kfs.size() == 1) {
        posOut = {kfs[0].px, kfs[0].py, kfs[0].pz};
        rotOut = {kfs[0].qx, kfs[0].qy, kfs[0].qz, kfs[0].qw};
        return true;
    }

    float frame = mCurrentFrame;
    if (frame <= (float)kfs.front().frame) {
        posOut = {kfs.front().px, kfs.front().py, kfs.front().pz};
        rotOut = {kfs.front().qx, kfs.front().qy, kfs.front().qz, kfs.front().qw};
        return true;
    }
    if (frame >= (float)kfs.back().frame) {
        posOut = {kfs.back().px, kfs.back().py, kfs.back().pz};
        rotOut = {kfs.back().qx, kfs.back().qy, kfs.back().qz, kfs.back().qw};
        return true;
    }

    // Find surrounding keyframes
    int left = 0, right = 0;
    for (size_t i = 0; i + 1 < kfs.size(); ++i) {
        if (kfs[i].frame <= (int)frame && (int)frame <= kfs[i + 1].frame) {
            left = (int)i; right = (int)i + 1; break;
        }
    }

    const auto& kl = kfs[left];
    const auto& kr = kfs[right];
    float diff = (float)(kr.frame - kl.frame);
    if (diff == 0) {
        posOut = {kl.px, kl.py, kl.pz};
        rotOut = {kl.qx, kl.qy, kl.qz, kl.qw};
        return true;
    }

    float t = (frame - kl.frame) / diff;

    // Bezier x, y, z, rotation time warping
    float tx = VmdInterp::interpBezier(t, kl.interpolation, 0);
    float ty = VmdInterp::interpBezier(t, kl.interpolation, 1);
    float tz = VmdInterp::interpBezier(t, kl.interpolation, 2);
    float tr = VmdInterp::interpBezier(t, kl.interpolation, 3);

    posOut[0] = VmdInterp::lerp(kl.px, kr.px, tx);
    posOut[1] = VmdInterp::lerp(kl.py, kr.py, ty);
    posOut[2] = VmdInterp::lerp(kl.pz, kr.pz, tz);

    std::array<float, 4> ql = {kl.qx, kl.qy, kl.qz, kl.qw};
    std::array<float, 4> qr = {kr.qx, kr.qy, kr.qz, kr.qw};
    rotOut = VmdInterp::slerpQuat(ql, qr, tr);
    return true;
}

float VmdPlayer::getMorphWeight(const std::string& morphName) const
{
    auto it = mAnimation.morphKeyframes.find(morphName);
    if (it == mAnimation.morphKeyframes.end()) return 0;

    const auto& kfs = it->second;
    if (kfs.empty()) return 0;
    if (kfs.size() == 1) return kfs[0].weight;

    float frame = mCurrentFrame;
    if (frame <= kfs.front().frame) return kfs.front().weight;
    if (frame >= kfs.back().frame) return kfs.back().weight;

    for (size_t i = 0; i + 1 < kfs.size(); ++i) {
        if (kfs[i].frame <= (int)frame && (int)frame <= kfs[i + 1].frame) {
            float diff = (float)(kfs[i + 1].frame - kfs[i].frame);
            if (diff == 0) return kfs[i].weight;
            float t = (frame - kfs[i].frame) / diff;
            return VmdInterp::lerp(kfs[i].weight, kfs[i + 1].weight, t);
        }
    }
    return 0;
}

// --- VmdMixer ---

VmdMixer::VmdMixer(float fps) : mFps(fps) {}

void VmdMixer::addVmd(VmdAnimation anim)
{
    mMaxFrame = std::max(mMaxFrame, (float)anim.maxFrame);
    mPlayers.emplace_back(std::move(anim), mFps);
}

void VmdMixer::clear() { mPlayers.clear(); mMaxFrame = 0; }

void VmdMixer::play()
{
    mPlaying = true;
    for (auto& p : mPlayers) p.play();
}

void VmdMixer::pause()
{
    mPlaying = false;
    for (auto& p : mPlayers) p.pause();
}

void VmdMixer::stop()
{
    mPlaying = false;
    for (auto& p : mPlayers) p.stop();
}

void VmdMixer::update(float deltaTime)
{
    if (!mPlaying) return;
    for (auto& p : mPlayers) {
        p.setLoop(mLoop);
        p.update(deltaTime);
    }
    // When all players finish and looping, reset all
    if (mLoop) {
        bool allDone = true;
        for (auto& p : mPlayers)
            if (p.currentFrame() < p.animation().maxFrame) { allDone = false; break; }
        if (allDone)
            for (auto& p : mPlayers) p.setFrame(0);
    }
}

bool VmdMixer::getBoneTransform(const std::string& boneName,
                                  std::array<float, 3>& posOut,
                                  std::array<float, 4>& rotOut) const
{
    bool has = false;
    std::array<float, 3> pos = {0, 0, 0};
    std::array<float, 4> rot;

    for (const auto& p : mPlayers) {
        std::array<float, 3> pp; std::array<float, 4> pr;
        if (p.getBoneTransform(boneName, pp, pr)) {
            pos[0] += pp[0]; pos[1] += pp[1]; pos[2] += pp[2];
            if (!has) rot = pr;
            else rot = VmdInterp::slerpQuat(rot, pr, 0.5f);
            has = true;
        }
    }
    if (!has) return false;
    posOut = pos;
    rotOut = rot;
    return true;
}

float VmdMixer::getMorphWeight(const std::string& morphName) const
{
    float sum = 0;
    bool has = false;
    for (const auto& p : mPlayers) {
        float w = p.getMorphWeight(morphName);
        if (w != 0) { sum += w; has = true; }
    }
    return has ? std::max(0.0f, std::min(1.0f, sum)) : 0;
}

void VmdMixer::setLoop(bool loop)
{
    mLoop = loop;
    for (auto& p : mPlayers) p.setLoop(loop);
}

void VmdMixer::setFrame(float frame)
{
    for (auto& p : mPlayers) p.setFrame(frame);
}
