#include "Model.h"
#include "pmx/PmxReader.h"
#include "render/opengl/ShaderManager.h"
#include "render/opengl/gpu/Shader.h"
#include "render/opengl/debug/RigidBodyRenderer.h"

#include <GL/glew.h>
#include <cmath>
#include <iostream>

namespace mmd {

void Model::load(const std::filesystem::path& pmxPath,
                 const std::filesystem::path& texDir,
                 const std::filesystem::path& toonDir)
{
    mData = PmxReader::load(pmxPath);
    std::cout << "Model: " << mData.name << " (" << mData.english_name << ")\n"
              << "Vertices: " << mData.vertexCount() << ", Faces: " << mData.faceCount()
              << ", Bones: " << mData.boneCount() << std::endl;

    mRenderer.loadModel(mData, texDir, toonDir);
    mPhysics.build(mData, mRenderer.modelScale());

    mRenderer.useSkinning = true;
    mRenderer.setupSkinning(mData);

    mBindPoseWorld = BoneSkinning::computePoseWorldMatrices(mData);
    mPoseWorld = mBindPoseWorld;
    mPhysics.resetPhysics(mPoseWorld);
    mPhysics.getBoneTransforms(mPoseWorld);

    mMorphCtl.setModel(mData);
}

void Model::loadVpd(const std::filesystem::path& vpdPath)
{
    mVpdPath = vpdPath;
    if (!vpdPath.empty() && std::filesystem::exists(vpdPath)) {
        mVpdPoses = VpdLoader::load(vpdPath);
        mVpdApplied = true;
        std::cout << "VPD: " << mVpdPoses.size() << " poses" << std::endl;
        mRenderer.setupSkinning(mData, vpdPath);
    } else {
        mRenderer.setupSkinning(mData);
    }
    mPoseWorld = BoneSkinning::computePoseWorldMatrices(mData, mVpdPoses);
    mPhysics.resetPhysics(mPoseWorld);
    mPhysics.getBoneTransforms(mPoseWorld);
}

void Model::update(float dt)
{
    if (mVmdMixer) {
        mVmdMixer->update(dt);
        std::unordered_map<std::string, std::pair<std::array<float,3>, std::array<float,4>>> vmdT;
        for (const auto& bone : mData.bones) {
            std::array<float,3> pos; std::array<float,4> rot;
            if (mVmdMixer->getBoneTransform(bone.name, pos, rot))
                vmdT[bone.name] = {pos, rot};
        }
        if (!vmdT.empty()) {
            if (mVpdApplied)
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mData, mVpdPoses, vmdT);
            else
                mPoseWorld = BoneSkinning::computePoseWorldMatrices(mData, {}, vmdT);
        }
        // Apply VMD morph weights
        std::unordered_map<std::string, float> vmdMorphs;
        for (const auto& m : mData.morphs) {
            float w = mVmdMixer->getMorphWeight(m.name);
            if (w != 0) vmdMorphs[m.name] = w;
        }
        if (!vmdMorphs.empty()) mMorphCtl.setMorphWeights(vmdMorphs);
    }

    mPhysics.updateMode0Bodies(mRenderer.useSkinning ? mPoseWorld : mBindPoseWorld);
    if (mPhysics.enabled) {
        mPhysics.step(dt, mPoseWorld);
        mPhysics.getBoneTransforms(mPoseWorld);
        BoneSkinning::recomputeAfterPhysicsBones(mData, mVpdPoses, mPoseWorld);
    }

    if (mIdleEnabled && (!mVmdMixer || !mVmdMixer->playing())) {
        mIdleTime += dt;
    }

    syncBoneTexture();
}

void Model::draw(ShaderManager& shaders,
                 const std::array<float, 16>& proj, const std::array<float, 16>& view,
                 const float* cameraPos)
{
    // Morph offset sync
    syncMorphOffsets();
    mRenderer.clearMaterialOverrides();
    for (size_t i = 0; i < mData.materials.size(); ++i) {
        if (auto* ov = mMorphCtl.getMaterialOverride((int)i))
            mRenderer.setMaterialOverride((int)i, *ov);
    }

    if (mRenderer.useSkinning) {
        bool useMorph = mMorphCtl.hasActiveMorphs();
        // Outline first (front-face cull + extrusion, must draw before main to not be depth-tested away)
        const char* ol = useMorph ? "morph_outline" : "outline_skinned";
        if (auto* s = shaders.get(ol))
            useMorph ? mRenderer.renderMorphOutlinePass(*s, proj, view)
                     : mRenderer.renderSkinnedOutlinePass(*s, proj, view);
        // Main model
        const char* sn = useMorph ? (mRenderer.showToon ? "morph" : "morph_notoon")
                                  : (mRenderer.showToon ? "skinned" : "skinned_notoon");
        if (auto* s = shaders.get(sn)) {
            if (mRenderer.showToon && cameraPos) {
                s->use();
                s->setVec3("camera_pos", cameraPos[0], cameraPos[1], cameraPos[2]);
                s->setFloat("shadow_thresh", 0.0f);
                s->setFloat("rim_power", 4.0f);
                s->setVec3("rim_color", 1.0f, 1.0f, 1.0f);
                s->setInt("gradient_map", 2);
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, shaders.gradientTexture()->id);
            }
            useMorph ? mRenderer.renderMorphMainPass(*s, proj, view)
                     : mRenderer.renderSkinnedMainPass(*s, proj, view);
        }
    } else {
        if (auto* s = shaders.get("outline"))
            mRenderer.renderOutlinePass(*s, proj, view);
        auto* sn = mRenderer.showToon ? "toon" : "main";
        if (auto* s = shaders.get(sn)) {
            if (mRenderer.showToon && cameraPos) {
                s->use();
                s->setVec3("camera_pos", cameraPos[0], cameraPos[1], cameraPos[2]);
                s->setFloat("shadow_thresh", 0.0f);
                s->setFloat("rim_power", 4.0f);
                s->setVec3("rim_color", 1.0f, 1.0f, 1.0f);
                s->setInt("gradient_map", 1);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, shaders.gradientTexture()->id);
            }
            mRenderer.renderMainPass(*s, proj, view);
        }
    }
}

void Model::drawPhysicsDebug(ShaderManager& shaders,
                             const std::array<float, 16>& proj, const std::array<float, 16>& view)
{
    // Lazy init — needs active GL context
    if (!mShowPhysicsDebug) return;
    if (!mPhysicsDebug) {
        mPhysicsDebug = std::make_unique<RigidBodyRenderer>();
        mPhysicsDebug->build(mData, mRenderer.modelScale());
        mPhysicsDebug->showRigidBody = true;
        mPhysicsDebug->showJoint = true;
        mPhysicsDebug->useBoneMatrices = false;
    }
    if (!mPhysicsDebug->showRigidBody) return;
    if (auto* s = shaders.get("rigidbody")) {
        mPhysicsDebug->updateFromPhysics(mPhysics);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glLineWidth(2.0f);
        mPhysicsDebug->render(*s, proj, view, mRenderer.modelMatrix());
        glLineWidth(1.0f);
    }
}

void Model::enablePhysics(bool on) { mPhysics.enabled = on; }

void Model::setVmd(std::unique_ptr<VmdMixer> mixer)
{
    mVmdMixer = std::move(mixer);
    if (mVmdMixer) mVmdMixer->play();
}

void Model::applyVpd(bool on)
{
    if (mVpdPoses.empty()) return;
    mVpdApplied = on;
    mPoseWorld = BoneSkinning::computePoseWorldMatrices(mData, on ? mVpdPoses : std::unordered_map<std::string, VpdPose>{});
    mPhysics.resetPhysics(mPoseWorld);
    syncBoneTexture();
}

void Model::setMorphWeight(const std::string& name, float weight)
{
    std::cout << "Morph: " << name << " weight=" << weight << std::endl;
    mMorphCtl.setMorphWeight(name, weight);
}

void Model::clearMorphs()
{
    mMorphCtl.clearMorphs();
}

void Model::setMorphWeights(const std::unordered_map<std::string, float>& weights)
{
    mMorphCtl.setMorphWeights(weights);
}

void Model::syncBoneTexture()
{
    auto& bm = mMorphCtl.boneMorphs();
    mRenderer.updateBoneTexture(mData, mPoseWorld, bm.empty() ? nullptr : &bm);
}

void Model::syncMorphOffsets()
{
    bool idleActive = mIdleEnabled && (!mVmdMixer || !mVmdMixer->playing());
    if (idleActive) {
        float blinkPhase = fmodf(mIdleTime, 4.0f);
        float w = 0;
        if (blinkPhase < 0.15f) {
            float t = blinkPhase / 0.15f;
            w = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
        }
        for (auto& nm : {"blink", "blink_l", "blink_r", "まばたき", "まぶたき", "ウィンク", "ｳｨﾝｸ"})
            mMorphCtl.morphWeights()[nm] = w;
        mMorphCtl.updateMorphOffsets();
    }
    if (mMorphCtl.offsetsChanged()) {
        mRenderer.morphVbo()->write(mMorphCtl.positionOffsets().data(),
            mMorphCtl.positionOffsets().size() * sizeof(float));
        if (auto* uv = mRenderer.uvMorphVbo())
            uv->write(mMorphCtl.uvOffsets().data(), mMorphCtl.uvOffsets().size() * sizeof(float));
        mMorphCtl.clearOffsetsChanged();
    }
}

} // namespace mmd
