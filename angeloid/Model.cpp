#include "Model.h"

#include "framework/MMD.h"
#include "core/pmx/PmxReader.h"
#include "framework/opengl/Pipeline.h"
#include "core/util/Log.h"

#include <cmath>

namespace mmd {

void Model::load(const std::filesystem::path& pmxPath) {
    mPmx = PmxReader::load(pmxPath);
    MMD_INFO("MODEL", "%s (%s)", mPmx.name.c_str(), mPmx.english_name.c_str());
    MMD_INFO("MODEL", "Vertices: %d, Faces: %d, Bones: %d", mPmx.vertexCount(), mPmx.faceCount(),
             mPmx.boneCount());

    mRenderer.loadModel(mPmx, pmxPath.parent_path());
    mPhysics.build(mPmx, mRenderer.modelScale());

    mVmdMixer = std::make_unique<VmdMixer>();

    mRenderer.setupSkinning(mPmx);

    mBindPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx);
    mPoseWorld = mBindPoseWorld;
    mInvBindPoseWorld = BoneSkinning::computeInvBindWorld(mBindPoseWorld);
    mPhysics.resetPhysics(mPoseWorld);
    mPhysics.getBoneTransforms(mPoseWorld);

    mMorphCtl.setModel(mPmx);

    // Detect head/eye/neck bones for lookAt tracking
    mBoneChildren.clear();
    mBoneChildren.resize(mPmx.boneCount());
    for (int i = 0; i < mPmx.boneCount(); ++i) {
        const auto& bone = mPmx.bones[i];
        const auto& name = bone.name;
        const auto& ename = bone.english_name;

        if (name == reinterpret_cast<const char*>(u8"頭") || ename == "head")
            mHeadBoneIndex = i;
        else if (name == reinterpret_cast<const char*>(u8"首") || ename == "neck")
            mNeckBoneIndex = i;
        else if ((name == reinterpret_cast<const char*>(u8"左目") || ename == "left eye") &&
                 mLeftEyeBoneIndex < 0)
            mLeftEyeBoneIndex = i;
        else if ((name == reinterpret_cast<const char*>(u8"右目") || ename == "right eye") &&
                 mRightEyeBoneIndex < 0)
            mRightEyeBoneIndex = i;

        int p = bone.parent_index;
        if (p >= 0 && p < mPmx.boneCount())
            mBoneChildren[p].push_back(i);
    }
    if (mHeadBoneIndex >= 0)
        MMD_INFO("MODEL", "LookAt bones: head=%d neck=%d leftEye=%d rightEye=%d", mHeadBoneIndex,
                 mNeckBoneIndex, mLeftEyeBoneIndex, mRightEyeBoneIndex);
}

int Model::loadVpd(const std::filesystem::path& vpdPath) {
    if (!std::filesystem::exists(vpdPath))
        return -1;
    auto poses = VpdLoader::load(vpdPath);
    int id = mVpdNextId++;
    MMD_INFO("MODEL", "VPD loaded [id=%d]: %zu poses (%s)", id, poses.size(),
             reinterpret_cast<const char*>(vpdPath.filename().u8string().c_str()));
    mVpdPoses.push_back({id, std::move(poses)});
    return id;
}

void Model::applyVpd(int vpdId) {
    MMD_INFO("MODEL", "VPD apply [id=%d]", vpdId);
    for (auto& [id, poses] : mVpdPoses) {
        if (id == vpdId) {
            mActiveVpdId = vpdId;
            mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, poses);
            if (mPhysics.enabled)
                mPhysics.resetPhysics(mPoseWorld);
            syncBoneTexture();
            return;
        }
    }
}

void Model::resetPose() {
    if (mVmdMixer->playing()) {
        MMD_INFO("MODEL", "Cannot reset pose while VMD is playing");
        return;
    }
    mClearVmd = true;
    MMD_INFO("MODEL", "Pose reset to bind pose");
    mActiveVpdId = -1;
    mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx);
    if (mPhysics.enabled)
        mPhysics.resetPhysics(mPoseWorld);
    syncBoneTexture();
}

void Model::syncVpdPose() {
    if (mVmdMixer->playing()) {
        MMD_INFO("MODEL", "Cannot sync VPD pose while VMD is playing");
        return;
    }
    mClearVmd = true;
    if (mActiveVpdId >= 0) {
        for (auto& [id, poses] : mVpdPoses)
            if (id == mActiveVpdId) {
                MMD_INFO("MODEL", "VPD pose sync [id=%d]", id);
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, poses);
                syncBoneTexture();
                return;
            }
    }
    MMD_INFO("MODEL", "VPD pose sync: no active VPD, using bind pose");
    mPoseWorld = mBindPoseWorld;
    syncBoneTexture();
}

void Model::removeVpd(int vpdId) {
    mVpdPoses.erase(std::remove_if(mVpdPoses.begin(), mVpdPoses.end(),
                                   [vpdId](auto& p) {
                                       return p.first == vpdId;
                                   }),
                    mVpdPoses.end());
    if (mActiveVpdId == vpdId)
        resetPose();
}

// Per-frame update pipeline:
//   1. VMD mixer: advance animation frames, compute bone + morph transforms
//   2. Physics:   mode 0 bodies follow bones -> step simulation -> write back bone transforms
//   3. Idle:      track idle time for auto-blink morph
//   4. GPU sync:  pack pose world matrices into bone texture for vertex shader skinning
void Model::update(float dt) {
    bool vmdUpdated = mVmdMixer->update(dt);
    if (vmdUpdated || !mClearVmd || mLookAtEnabled) {
        static int rebuildFrame = 0;
        ++rebuildFrame;
        MMD_DEBUG("LOOKAT", "rebuild poseWorld frame=%d (vmd=%d clear=%d lookAt=%d)", rebuildFrame,
                  (int)vmdUpdated, (int)!mClearVmd, (int)mLookAtEnabled);
        mVmdBoneCache.clear();
        for (const auto& bone : mPmx.bones) {
            std::array<float, 3> pos;
            std::array<float, 4> rot;
            if (mVmdMixer->getBoneTransform(bone.name, pos, rot))
                mVmdBoneCache[bone.name] = {pos, rot};
        }

        if (!mVmdBoneCache.empty()) {
            if (mActiveVpdId >= 0) {
                for (auto& [id, poses] : mVpdPoses) {
                    if (id == mActiveVpdId) {
                        mPoseWorld =
                            BoneSkinning::computePoseWorldMatrices(mPmx, poses, mVmdBoneCache);
                        break;
                    }
                }
            }
            else {
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, {}, mVmdBoneCache);
            }
        }
        else if (mLookAtEnabled) {
            // Rebuild pose world from VPD-only (no VMD) so lookAt always
            // starts from the canonical pose each frame.
            if (mActiveVpdId >= 0) {
                for (auto& [id, poses] : mVpdPoses) {
                    if (id == mActiveVpdId) {
                        mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, poses);
                        break;
                    }
                }
            }
            else {
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx);
            }
        }
        mVmdMorphCache.clear();
        for (const auto& m : mPmx.morphs) {
            float w = mVmdMixer->getMorphWeight(m.name);
            if (w != 0)
                mVmdMorphCache[m.name] = w;
        }
        if (!mVmdMorphCache.empty())
            mMorphCtl.setMorphWeights(mVmdMorphCache);
    }

    if (mLookAtEnabled)
        applyLookAt();

    mPhysics.updateMode0Bodies(mPoseWorld);
    if (mPhysics.enabled) {
        mPhysics.step(dt, mPoseWorld);
        mPhysics.getBoneTransforms(mPoseWorld);
        {
            VpdPoseMap emptyVpd;
            VpdPoseMap* activeVpd = &emptyVpd;
            for (auto& [id, poses] : mVpdPoses)
                if (id == mActiveVpdId) {
                    activeVpd = &poses;
                    break;
                }
            BoneSkinning::recomputeAfterPhysicsBones(mPmx, *activeVpd, mPoseWorld);
        }

        static int checkFrame = 0;
        if (++checkFrame % 60 == 0) {
            int nanCount = 0;
            for (size_t i = 0; i < mPoseWorld.size(); i++) {
                for (int j = 0; j < 16; j++) {
                    if (std::isnan(mPoseWorld[i][j])) {
                        nanCount++;
                        break;
                    }
                }
            }
            if (nanCount > 0) {
                MMD_ERROR("MODEL", "frame=%d NaN in %d/%d pose matrices!", checkFrame, nanCount,
                          (int)mPoseWorld.size());
            }
        }
        // Periodic physics analysis (press F to dump manually via debugDump instead)
        // static int dumpFrame = 0;
        // if (++dumpFrame % 120 == 0) {
        //     mPhysics.debugFullDump(dumpFrame);
        // }
    }

    // --- Idle animation ---
    if (mIdleEnabled && (!mVmdMixer || !mVmdMixer->playing())) {
        mIdleTime += dt;
    }

    // --- Upload bone matrices to GPU ---
    syncBoneTexture();
}

void Model::draw(int screenWidth, int screenHeight) {
    Pipeline::instance().resizeViewport(screenWidth, screenHeight);

    auto proj = Camera::projectionMatrix(screenWidth, screenHeight);
    auto view = Camera::instance().viewMatrix();
    float camPos[3];
    Camera::instance().getEyePosition(camPos[0], camPos[1], camPos[2]);

    // Morph offset sync
    syncMorphOffsets();
    mRenderer.clearMaterialOverrides();
    for (size_t i = 0; i < mPmx.materials.size(); ++i) {
        if (auto* ov = mMorphCtl.getMaterialOverride((int)i))
            mRenderer.setMaterialOverride((int)i, *ov);
    }

    // Lazy-init physics debug
    if (mShowRigidBodies && !mPhysicsDebug) {
        mPhysicsDebug = std::make_unique<RigidBodyRenderer>();
        mPhysicsDebug->build(mPmx, mRenderer.modelScale());
        mPhysicsDebug->showRigidBody = true;
        mPhysicsDebug->showJoint = true;
        mPhysicsDebug->useBoneMatrices = false;
    }
    
    // Light-space view-projection for shadow map.
    // Key light from upper-front-right (classic MMD lighting).
    Vec3 ld = {0.3f, 0.8f, 0.5f};
    float l = std::sqrt(ld.x*ld.x + ld.y*ld.y + ld.z*ld.z);
    ld = {ld.x/l, ld.y/l, ld.z/l};

    std::array<float, 16> lightViewProj;
    {

        // Light position 15 units away from origin in light direction
        float dist = 15.0f;
        Vec3 lp = {ld.x * dist, ld.y * dist, ld.z * dist};

        // Simple orthographic: light looks toward origin
        // Build view matrix (column-major): maps world coords to light-eye coords
        Vec3 fwd = {-ld.x, -ld.y, -ld.z};
        Vec3 worldUp = {0, 1, 0};
        if (std::abs(fwd.x) < 0.001f && std::abs(fwd.z) < 0.001f)
            worldUp = {1, 0, 0};
        Vec3 r = {worldUp.y*fwd.z - worldUp.z*fwd.y,
                  worldUp.z*fwd.x - worldUp.x*fwd.z,
                  worldUp.x*fwd.y - worldUp.y*fwd.x};
        float rl = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
        r = {r.x/rl, r.y/rl, r.z/rl};
        Vec3 u = {fwd.y*r.z - fwd.z*r.y,
                  fwd.z*r.x - fwd.x*r.z,
                  fwd.x*r.y - fwd.y*r.x};

        // Orthographic projection (column-major)
        float size = 6.0f;  // half-size of ortho frustum
        float zn = 0.1f, zf = 50.0f;
        float sx = 1.0f / size, sy = 1.0f / size;
        float sz = 2.0f / (zn - zf);
        float tz = -(zf + zn) / (zf - zn);

        // View (column-major): translate by -lp, then rotate by [r,u,fwd]
        // Combined VP: projection * view
        lightViewProj = {
            r.x * sx,        u.x * sy,        fwd.x * sz,     0,
            r.y * sx,        u.y * sy,        fwd.y * sz,     0,
            r.z * sx,        u.z * sy,        fwd.z * sz,     0,
            -(r.x*lp.x + r.y*lp.y + r.z*lp.z) * sx,
            -(u.x*lp.x + u.y*lp.y + u.z*lp.z) * sy,
            -(fwd.x*lp.x + fwd.y*lp.y + fwd.z*lp.z) * sz + tz,
            1.0f
        };
    }

    Pipeline::FrameParams fp;
    fp.proj = &proj;
    fp.view = &view;
    fp.modelMat = mRenderer.modelMatrix();
    fp.camPosX = camPos[0];
    fp.camPosY = camPos[1];
    fp.camPosZ = camPos[2];
    fp.lightDirX = ld.x;
    fp.lightDirY = ld.y;
    fp.lightDirZ = ld.z;
    fp.lightViewProj = &lightViewProj;
    fp.showToon = mRenderer.showToon;
    fp.showRigidBodies = mShowRigidBodies;
    fp.physicsDebug = mPhysicsDebug.get();
    fp.physics = &mPhysics;

    Pipeline::instance().execute(mRenderer, fp);
}

void Model::enablePhysics(bool on) {
    mPhysics.enabled = on;
}

// --- VMD ---

int Model::loadVmd(const std::filesystem::path& path) {
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

void Model::playVmd(int trackId, std::function<void(int)> onEnd) {
    MMD_INFO("MODEL", "VMD play [id=%d]%s", trackId, onEnd ? " with callback" : "");
    mClearVmd = false;
    mVmdMixer->play(trackId, std::move(onEnd));
}
void Model::pauseVmd(int trackId) {
    MMD_INFO("MODEL", "VMD pause [id=%d]", trackId);
    mVmdMixer->pause(trackId);
}
void Model::stopVmd(int trackId) {
    MMD_INFO("MODEL", "VMD stop [id=%d]", trackId);
    mVmdMixer->stop(trackId);
}
void Model::removeVmd(int trackId) {
    MMD_INFO("MODEL", "VMD remove [id=%d]", trackId);
    mVmdMixer->removeVmd(trackId);
}

void Model::playAllVmd() {
    MMD_INFO("MODEL", "VMD play all");
    mClearVmd = false;
    mVmdMixer->playAll();
}
void Model::pauseAllVmd() {
    MMD_INFO("MODEL", "VMD pause all");
    mVmdMixer->pauseAll();
}
void Model::stopAllVmd() {
    MMD_INFO("MODEL", "VMD stop all");
    mVmdMixer->stopAll();
}
bool Model::isVmdPlaying() const {
    return mVmdMixer->playing();
}

int Model::vmdTrackCount() const {
    return mVmdMixer ? mVmdMixer->trackCount() : 0;
}
bool Model::isVmdPlaying(int trackId) const {
    return mVmdMixer && mVmdMixer->playing(trackId);
}
float Model::vmdCurrentFrame(int trackId) const {
    return mVmdMixer ? mVmdMixer->currentFrame(trackId) : 0;
}
float Model::vmdMaxFrame(int trackId) const {
    return vmdCurrentFrame(trackId);  // simplfied
}

void Model::setVmdFrame(int trackId, float frame) {
    mVmdMixer->setFrame(trackId, frame);
}

// --- Morph iteration ---

int Model::morphCount() const {
    return mPmx.morphCount();
}

std::string Model::morphName(int index) const {
    if (index < 0 || index >= mPmx.morphCount())
        return {};
    return mPmx.morphs[index].name;
}

std::vector<int> Model::interactableMorphs() const {
    std::vector<int> result;
    for (int i = 0; i < mPmx.morphCount(); ++i) {
        int t = mPmx.morphs[i].morph_type;
        if (t == MORPH_TYPE_VERTEX || t == MORPH_TYPE_GROUP || t == MORPH_TYPE_MATERIAL ||
            t == MORPH_TYPE_UV || t == MORPH_TYPE_BONE)
            result.push_back(i);
    }
    return result;
}

void Model::setMorphWeight(const std::string& name, float weight) {
    MMD_INFO("MORPH", "%s = %.2f", name.c_str(), weight);
    mSavedWeights[name] = weight;
    mMorphCtl.setMorphWeight(name, weight);
}

float Model::savedMorphWeight(const std::string& name) const {
    auto it = mSavedWeights.find(name);
    return it != mSavedWeights.end() ? it->second : 0.0f;
}

void Model::clearMorphs() {
    mMorphCtl.clearMorphs();
}

void Model::setMorphWeights(const std::unordered_map<std::string, float>& weights) {
    mMorphCtl.setMorphWeights(weights);
}

void Model::syncBoneTexture() {
    auto skinMatrices = BoneSkinning::computeSkinningMatrices(
        mPoseWorld, mInvBindPoseWorld, mPmx.boneCount());
    auto& bm = mMorphCtl.boneMorphs();
    if (!bm.empty())
        BoneSkinning::applyBoneMorphs(skinMatrices, mPmx.boneCount(), bm, mRenderer.modelScale());
    auto data = BoneSkinning::packBoneMatrices(skinMatrices, mPmx.boneCount());
    mRenderer.uploadBoneData(data.pixels.data(), data.pixels.size() * sizeof(float));
}

void Model::syncMorphOffsets() {
    bool idleActive = mIdleEnabled && (!mVmdMixer || !mVmdMixer->playing());
    // Idle auto-blink: every 4 seconds, a 0.15s triangular blink waveform.
    // Try multiple common morph names to handle different model naming conventions.
    if (idleActive) {
        float blinkPhase = fmodf(mIdleTime, 4.0f);
        float w = 0;
        if (blinkPhase < 0.15f) {
            float t = blinkPhase / 0.15f;
            w = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;  // Triangle wave: 0->1->0
        }
        for (auto& nm : initArgs().blinkMorphs)
            mMorphCtl.morphWeights()[nm] = w;
        mMorphCtl.updateMorphOffsets();
    }
    mRenderer.morphVbo()->write(mMorphCtl.positionOffsets().data(),
                                mMorphCtl.positionOffsets().size() * sizeof(float));
    if (auto* uv = mRenderer.uvMorphVbo())
        uv->write(mMorphCtl.uvOffsets().data(), mMorphCtl.uvOffsets().size() * sizeof(float));
}

void Model::lookAt(int screenX, int screenY, int screenW, int screenH) {
    mLookAtEnabled = true;
    mLookAtScreenX = screenX;
    mLookAtScreenY = screenY;
    mLookAtScreenW = screenW > 0 ? screenW : 1;
    mLookAtScreenH = screenH > 0 ? screenH : 1;
}

void Model::resetLookAt() {
    mLookAtEnabled = false;
}

void Model::propagateToDescendants(int parentIdx, const Mat4& deltaWorld) {
    std::vector<int> stack;
    for (int c : mBoneChildren[parentIdx])
        stack.push_back(c);
    while (!stack.empty()) {
        int child = stack.back();
        stack.pop_back();
        mPoseWorld[child] = mat4Mul(deltaWorld, mPoseWorld[child]);
        for (int gc : mBoneChildren[child])
            stack.push_back(gc);
    }
}

void Model::applyBoneQuat(int boneIdx, const Quat& qFull, float angleScale) {
    if (boneIdx < 0 || boneIdx >= (int)mPoseWorld.size())
        return;

    for (int j = 0; j < 16; ++j)
        if (std::isnan(mPoseWorld[boneIdx][j]))
            return;

    float halfAngle = std::acos(std::max(-1.0f, std::min(1.0f, qFull.w)));
    if (halfAngle < 1e-6f)
        return;

    float angle = 2.0f * halfAngle;
    float scaledAngle = angle * angleScale;
    if (scaledAngle < 1e-6f)
        return;

    static constexpr float kMaxAngle = 50.0f * 3.14159265f / 180.0f;
    if (scaledAngle > kMaxAngle * 1.5f)
        scaledAngle = kMaxAngle * 1.5f;

    float sinHalf = std::sin(halfAngle);
    float ratio = std::sin(scaledAngle * 0.5f) / sinHalf;
    Quat q = quatNormalize(
        {qFull.x * ratio, qFull.y * ratio, qFull.z * ratio, std::cos(scaledAngle * 0.5f)});

    // Apply world-space rotation around bone's pivot
    Mat4 R = mat4FromQuat(q);
    Mat4 oldWorld = mPoseWorld[boneIdx];
    Mat4 newWorld = mat4Mul(R, oldWorld);
    newWorld[12] = oldWorld[12];
    newWorld[13] = oldWorld[13];
    newWorld[14] = oldWorld[14];
    mPoseWorld[boneIdx] = newWorld;

    Mat4 deltaWorld = mat4Mul(newWorld, mat4InverseAffine(oldWorld));
    propagateToDescendants(boneIdx, deltaWorld);
}

void Model::applyLookAt() {
    if (mHeadBoneIndex < 0 || mLookAtScreenW <= 0 || mLookAtScreenH <= 0)
        return;

    // Camera axes for world-space rotation reference
    auto& cam = Camera::instance();
    Vec3 camPos;
    cam.getEyePosition(camPos.x, camPos.y, camPos.z);
    auto view = cam.viewMatrix();
    Vec3 right = {view[0], view[4], view[8]};
    Vec3 upVec = {view[1], view[5], view[9]};
    Vec3 backward = {view[2], view[6], view[10]};

    // Head world position
    Vec3 headPosModel = mat4Translation(mPoseWorld[mHeadBoneIndex]);
    const float* mm = mRenderer.modelMatrix();
    Mat4 modelMat = {mm[0], mm[1], mm[2],  mm[3],  mm[4],  mm[5],  mm[6],  mm[7],
                     mm[8], mm[9], mm[10], mm[11], mm[12], mm[13], mm[14], mm[15]};
    Vec3 headPosWorld = mat4TransformPoint(modelMat, headPosModel);

    // Project head to NDC
    float aspect = (float)mLookAtScreenW / (float)mLookAtScreenH;
    float tanHalfFov = std::tan(45.0f * 3.14159265f / 360.0f);
    Vec3 headView = {vec3Dot(vec3Sub(headPosWorld, camPos), right),
                     vec3Dot(vec3Sub(headPosWorld, camPos), upVec),
                     vec3Dot(vec3Sub(headPosWorld, camPos), backward)};  // backward = camera -Z
    float headViewZ = -headView.z;
    if (headViewZ < 0.1f)
        headViewZ = 5.0f;
    float headNdcX =
        -(headView.x / headViewZ) / (tanHalfFov * aspect);  // mirror to match mouseNdcX
    float headNdcY = (headView.y / headViewZ) / tanHalfFov;

    // Mouse NDC — Orbit mode derives target from camera eye position
    float mouseNdcX = 1.0f - (2.0f * mLookAtScreenX) / mLookAtScreenW;
    float mouseNdcY = 1.0f - (2.0f * mLookAtScreenY) / mLookAtScreenH;

    // Relative NDC = offset from head; ±1 → ±maxAngle
    static constexpr float kMaxAngle = 50.0f * 3.14159265f / 180.0f;

    if (cam.mode() == CameraMode::Orbit)
        return;  // Orbit mode: skip lookAt

    // FPS mode: NDC-based mouse tracking
    float relNdcX = mouseNdcX - headNdcX;
    float relNdcY = mouseNdcY - headNdcY;
    Quat qYaw = quatFromAxisAngle(upVec, relNdcX * kMaxAngle);
    Quat qPitch = quatFromAxisAngle(right, relNdcY * kMaxAngle);
    Quat qFull = quatNormalize(quatMul(qPitch, qYaw));

    applyBoneQuat(mNeckBoneIndex, qFull, 0.1f);
    applyBoneQuat(mHeadBoneIndex, qFull, 1.0f);
    applyBoneQuat(mLeftEyeBoneIndex, qFull, 0.2f);
    applyBoneQuat(mRightEyeBoneIndex, qFull, 0.2f);
}

}  // namespace mmd
