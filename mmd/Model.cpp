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

    mRenderer.useSkinning = true;
    mRenderer.setupSkinning(mPmx);

    mBindPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx);
    mPoseWorld = mBindPoseWorld;
    mPhysics.resetPhysics(mPoseWorld);
    mPhysics.getBoneTransforms(mPoseWorld);

    mMorphCtl.setModel(mPmx);
}

void Model::loadVpd(const std::filesystem::path& vpdPath) {
    mVpdPath = vpdPath;
    if (!vpdPath.empty() && std::filesystem::exists(vpdPath)) {
        mVpdPoses = VpdLoader::load(vpdPath);
        mVpdApplied = true;
        MMD_INFO("MODEL", "VPD: %zu poses", mVpdPoses.size());
        mRenderer.setupSkinning(mPmx, vpdPath);
    }
    else {
        mVpdPoses.clear();
        mRenderer.setupSkinning(mPmx);
    }
    mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, mVpdPoses);
    mPhysics.resetPhysics(mPoseWorld);
    mPhysics.getBoneTransforms(mPoseWorld);
}

// Per-frame update pipeline:
//   1. VMD mixer: advance animation frames, compute bone + morph transforms
//   2. Physics:   mode 0 bodies follow bones -> step simulation -> write back bone transforms
//   3. Idle:      track idle time for auto-blink morph
//   4. GPU sync:  pack pose world matrices into bone texture for vertex shader skinning
void Model::update(float dt) {
    // --- VMD animation ---
    if (mVmdMixer) {
        mVmdMixer->update(dt);
        // Collect per-bone transforms from all VMD layers
        std::unordered_map<std::string, std::pair<std::array<float, 3>, std::array<float, 4>>> vmdT;
        for (const auto& bone : mPmx.bones) {
            std::array<float, 3> pos;
            std::array<float, 4> rot;
            if (mVmdMixer->getBoneTransform(bone.name, pos, rot))
                vmdT[bone.name] = {pos, rot};
        }
        if (!vmdT.empty()) {
            if (mVpdApplied && !mVpdPoses.empty())
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, mVpdPoses, vmdT);
            else
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx, {}, vmdT);
        }
        // Collect morph weights from VMD
        std::unordered_map<std::string, float> vmdMorphs;
        for (const auto& m : mPmx.morphs) {
            float w = mVmdMixer->getMorphWeight(m.name);
            if (w != 0)
                vmdMorphs[m.name] = w;
        }
        if (!vmdMorphs.empty())
            mMorphCtl.setMorphWeights(vmdMorphs);
    }

    // --- Physics ---
    // Mode 0 (kinematic) bodies must follow their bones each frame before the step.
    // When skinning is off, use bind pose as the bone reference.
    mPhysics.updateMode0Bodies(mRenderer.useSkinning ? mPoseWorld : mBindPoseWorld);
    if (mPhysics.enabled) {
        mPhysics.step(dt, mPoseWorld);
        // Write physics results back into pose world matrices
        mPhysics.getBoneTransforms(mPoseWorld);
        // Recompute child bones that inherit physics-deformed parents
        BoneSkinning::recomputeAfterPhysicsBones(mPmx, mVpdPoses,
                                                 mPoseWorld);
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
                glLineWidth(2.0f);
                mPhysicsDebug->render(*s, proj, view, mRenderer.modelMatrix());
                glLineWidth(1.0f);
            }
        }
    }
}

void Model::enablePhysics(bool on) {
    mPhysics.enabled = on;
}

// --- VMD ---

void Model::loadVmd(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return;
    auto anim = VmdAnimation::load(path);
    MMD_INFO("MODEL", "VMD: %s (max frame: %d)", anim.modelName.c_str(), anim.maxFrame);
    mOwnedAnimations.push_back(std::move(anim));
    if (!mVmdMixer) {
        mVmdMixer = std::make_unique<VmdMixer>();
        mVmdMixer->play();
    }
    mVmdMixer->addVmd(&mOwnedAnimations.back());
}

bool Model::hasVmd() const {
    return mVmdMixer != nullptr;
}
void Model::vmdPlay() {
    if (mVmdMixer)
        mVmdMixer->play();
}
void Model::vmdPause() {
    if (mVmdMixer)
        mVmdMixer->pause();
}
bool Model::vmdPlaying() const {
    return mVmdMixer && mVmdMixer->playing();
}
bool Model::vmdLoop() const {
    return mVmdMixer && mVmdMixer->loop();
}
void Model::setVmdLoop(bool loop) {
    if (mVmdMixer)
        mVmdMixer->setLoop(loop);
}
float Model::vmdCurrentFrame() const {
    return mVmdMixer ? mVmdMixer->currentFrame() : 0;
}
float Model::vmdMaxFrame() const {
    return mVmdMixer ? mVmdMixer->maxFrame() : 0;
}

void Model::setVmdFrame(float frame) {
    if (mVmdMixer)
        mVmdMixer->setFrame(frame);
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

void Model::applyVpd(bool on) {
    if (mVpdPoses.empty())
        return;
    mVpdApplied = on;
    mPoseWorld = BoneSkinning::computePoseWorldMatrices(
        mPmx, on ? mVpdPoses : VpdPoseMap{});
    mPhysics.resetPhysics(mPoseWorld);
    syncBoneTexture();
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
