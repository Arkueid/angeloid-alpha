#include "Model.h"

#include "MMD.h"
#include "pmx/PmxReader.h"
#include "render/opengl/RenderContext.h"
#include "render/opengl/gpu/Shader.h"
#include "util/Log.h"

#include <GL/glew.h>
#include <cmath>
#include <set>

namespace mmd {

void Model::load(const std::filesystem::path& pmxPath) {
    mPmx = PmxReader::load(pmxPath);
    MMD_INFO("MODEL", "%s (%s)", mPmx.name.c_str(), mPmx.english_name.c_str());
    MMD_INFO("MODEL", "Vertices: %d, Faces: %d, Bones: %d", mPmx.vertexCount(), mPmx.faceCount(),
             mPmx.boneCount());

    mRenderer.loadModel(mPmx, pmxPath.parent_path());
    mPhysics.build(mPmx, mRenderer.modelScale());

    mVmdMixer = std::make_unique<VmdMixer>();

    mRenderer.useSkinning = true;
    mRenderer.setupSkinning(mPmx);

    mBindPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx);
    mPoseWorld = mBindPoseWorld;
    mPhysics.resetPhysics(mPoseWorld);
    mPhysics.getBoneTransforms(mPoseWorld);

    mMorphCtl.setModel(mPmx);
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
    if (mVmdMixer->update(dt) || !mClearVmd) {
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
                        mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, poses, mVmdBoneCache);
                        break;
                    }
                }
            }
            else {
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, {}, mVmdBoneCache);
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

    mPhysics.updateMode0Bodies(mRenderer.useSkinning ? mPoseWorld : mBindPoseWorld);
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
                MMD_ERROR("MODEL", "frame=%d NaN in %d/%d pose matrices!", checkFrame, nanCount, (int)mPoseWorld.size());
            }
        }
    }

    // --- Idle animation ---
    if (mIdleEnabled && (!mVmdMixer || !mVmdMixer->playing())) {
        mIdleTime += dt;
    }

    // --- Upload bone matrices to GPU ---
    syncBoneTexture();
}

void Model::draw(int screenWidth, int screenHeight) {
    auto proj = Camera::projectionMatrix(screenWidth, screenHeight);
    auto view = Camera::instance().viewMatrix();
    float camPos[3] = {Camera::instance().x, Camera::instance().y, Camera::instance().z};

    auto& ctx = RenderContext::instance();

    // Morph offset sync
    syncMorphOffsets();
    mRenderer.clearMaterialOverrides();
    for (size_t i = 0; i < mPmx.materials.size(); ++i) {
        if (auto* ov = mMorphCtl.getMaterialOverride((int)i))
            mRenderer.setMaterialOverride((int)i, *ov);
    }

    if (mRenderer.useSkinning) {
        bool useMorph = mMorphCtl.hasActiveMorphs();
        const char* ol = useMorph ? "morph_outline" : "outline_skinned";
        if (auto* s = ctx.shader(ol))
            useMorph ? mRenderer.renderMorphOutlinePass(*s, proj, view)
                     : mRenderer.renderSkinnedOutlinePass(*s, proj, view);
        const char* sn = useMorph ? (mRenderer.showToon ? "morph" : "morph_notoon")
                                  : (mRenderer.showToon ? "skinned" : "skinned_notoon");
        if (auto* s = ctx.shader(sn)) {
            if (mRenderer.showToon) {
                s->use();
                s->setVec3("camera_pos", camPos[0], camPos[1], camPos[2]);
                s->setFloat("shadow_thresh", 0.0f);
                s->setFloat("rim_power", 4.0f);
                s->setVec3("rim_color", 1.0f, 1.0f, 1.0f);
                s->setInt("gradient_map", 2);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, ctx.gradientTexture()->id);
            }
            useMorph ? mRenderer.renderMorphMainPass(*s, proj, view)
                     : mRenderer.renderSkinnedMainPass(*s, proj, view);
        }
    }
    else {
        if (auto* s = ctx.shader("outline"))
            mRenderer.renderOutlinePass(*s, proj, view);
        auto* sn = mRenderer.showToon ? "toon" : "main";
        if (auto* s = ctx.shader(sn)) {
            if (mRenderer.showToon) {
                s->use();
                s->setVec3("camera_pos", camPos[0], camPos[1], camPos[2]);
                s->setFloat("shadow_thresh", 0.0f);
                s->setFloat("rim_power", 4.0f);
                s->setVec3("rim_color", 1.0f, 1.0f, 1.0f);
                s->setInt("gradient_map", 1);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, ctx.gradientTexture()->id);
            }
            mRenderer.renderMainPass(*s, proj, view);
        }
    }

    // Rigid body debug overlay
    if (mShowRigidBodies) {
        if (!mPhysicsDebug) {
            mPhysicsDebug = std::make_unique<RigidBodyRenderer>();
            mPhysicsDebug->build(mPmx, mRenderer.modelScale());
            mPhysicsDebug->showRigidBody = true;
            mPhysicsDebug->showJoint = true;
            mPhysicsDebug->useBoneMatrices = false;
        }
        if (mPhysicsDebug->showRigidBody) {
            if (auto* s = ctx.shader("rigidbody")) {
                mPhysicsDebug->updateFromPhysics(mPhysics);
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                mPhysicsDebug->render(*s, proj, view, mRenderer.modelMatrix());
            }
        }
    }
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
    auto& bm = mMorphCtl.boneMorphs();
    mRenderer.updateBoneTexture(mPmx, mPoseWorld, bm.empty() ? nullptr : &bm);
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

}  // namespace mmd
