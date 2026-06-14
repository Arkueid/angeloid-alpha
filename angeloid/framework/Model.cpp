#include "Model.h"

#include "framework/Camera.h"
#include "framework/MMD.h"
#include "core/pmx/PmxReader.h"
#include "framework/opengl/RenderContext.h"
#include "framework/opengl/ShaderManager.h"
#include "framework/opengl/ShaderStandard.h"
#include "core/util/Log.h"

#include <cmath>

namespace mmd {

void Model::load(const std::filesystem::path& pmxPath) {
    mPmx = PmxReader::load(pmxPath);
    MMD_INFO("MODEL", "%s (%s)", mPmx.name.c_str(), mPmx.english_name.c_str());
    MMD_INFO("MODEL", "Vertices: %d, Faces: %d, Bones: %d", mPmx.vertexCount(), mPmx.faceCount(),
             mPmx.boneCount());

    mRenderer.loadModel(mPmx, pmxPath.parent_path());

    mBindPoseWorld = BoneSkinning::computePoseWorldMatrices(mPmx);
    mPhysics.build(mPmx, mRenderer.modelScale(), mBindPoseWorld);

    mAnimCtrl.init();

    mRenderer.setupSkinning(mPmx);
    mPoseWorld = mBindPoseWorld;
    mInvBindPoseWorld = BoneSkinning::computeInvBindWorld(mBindPoseWorld);
    mAnimCtrl.setBindPose(mBindPoseWorld);

    mPhysics.resetPhysics(mPoseWorld);
    mPhysics.getBoneTransforms(mPoseWorld);

    mMorphCtl.setModel(mPmx);
    mLookAtCtrl.setup(mPmx);
}

void Model::applyVpdPost() {
    if (mPhysics.enabled)
        mPhysics.resetPhysics(mPoseWorld);
    mBoneDirty = true;
    syncBoneTexture();
}

void Model::resetPosePost() {
    if (mPhysics.enabled)
        mPhysics.resetPhysics(mPoseWorld);
    mBoneDirty = true;
    syncBoneTexture();
}

void Model::update(float dt) {
    // 1. Animation: advance VMD, compute pose world, collect VMD morph weights
    auto result = mAnimCtrl.update(dt, mPmx, mPoseWorld);

    // 2. Apply VMD morph weights to MorphController
    if (!result.vmdMorphWeights.empty())
        mMorphCtl.setMorphWeights(result.vmdMorphWeights);

    // 3. LookAt
    if (mLookAtCtrl.enabled()) {
        std::array<float, 16> mm;
        std::memcpy(mm.data(), mRenderer.modelMatrix(), sizeof(mm));
        mLookAtCtrl.apply(mPoseWorld, mm);
    }

    // 4. Physics
    mPhysics.updateMode0Bodies(mPoseWorld);
    if (mPhysics.enabled) {
        mPhysics.step(dt, mPoseWorld);
        mPhysics.getBoneTransforms(mPoseWorld);
        if (result.activeVpd) {
            BoneSkinning::recomputeAfterPhysicsBones(mPmx, *result.activeVpd, mPoseWorld);
        } else {
            BoneSkinning::recomputeAfterPhysicsBones(mPmx, {}, mPoseWorld);
        }

#ifndef NDEBUG
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
#endif
    }

    // 5. GPU sync
    bool bonesChanged = result.bonesChanged || mLookAtCtrl.enabled() || mPhysics.enabled;
    mBoneDirty = mBoneDirty || bonesChanged;
    syncBoneTexture();
}

void Model::onShadowPass(const std::array<float, 16>& lightViewProj,
                         const std::array<float, 16>& /*model*/) {
    if (!mRenderer.showModel) return;

    syncMorphOffsets();
    mRenderer.clearMaterialOverrides();
    for (size_t i = 0; i < mPmx.materials.size(); ++i) {
        if (auto* ov = mMorphCtl.getMaterialOverride((int)i))
            mRenderer.setMaterialOverride((int)i, *ov);
    }

    auto* shadowProg = ShaderManager::instance().shadow();
    if (shadowProg)
        mRenderer.renderDepthPass(*shadowProg, lightViewProj, mRenderer.modelMatrix());
}

void Model::onMainPass(const std::array<float, 16>& proj,
                       const std::array<float, 16>& view,
                       const std::array<float, 16>& /*model*/,
                       const std::array<float, 16>& lightViewProj,
                       bool hasShadow) {
    if (!mRenderer.showModel) return;
    const float* mm = mRenderer.modelMatrix();

    auto& sm = ShaderManager::instance();

    if (mRenderer.showOutline) {
        auto* outlineProg = sm.outline();
        if (outlineProg)
            mRenderer.renderMorphOutlinePass(*outlineProg, proj, view, mm);
    }

    auto* mainProg = mRenderer.showToon ? sm.toon() : sm.main();
    if (!mainProg) return;

    mainProg->use();

    if (hasShadow) {
        mainProg->setInt("u_shadowMap", 5);
        mainProg->setMat4("u_lightViewProj", lightViewProj.data());
        mainProg->setInt("u_hasShadow", 1);
    } else {
        mainProg->setInt("u_hasShadow", 0);
    }

    mainProg->setVec3(U_LIGHT_DIR, 0.3f, 0.8f, 0.5f);

    if (mRenderer.showToon) {
        float camPos[3];
        Camera::instance().getEyePosition(camPos[0], camPos[1], camPos[2]);
        mainProg->setVec3(U_CAMERA_POS, camPos[0], camPos[1], camPos[2]);
        mainProg->setFloat(U_RIM_POWER, 4.0f);
        mainProg->setVec3(U_RIM_COLOR, 1.0f, 1.0f, 1.0f);

        mainProg->setInt("u_gradientMap", TEX_UNIT_GRADIENT);
        auto& ctx = mmd::RenderContext::instance();
        glActiveTexture(GL_TEXTURE0 + TEX_UNIT_GRADIENT);
        glBindTexture(GL_TEXTURE_2D, ctx.gradientTexture()->id);
    }

    mRenderer.renderMorphMainPass(*mainProg, proj, view, mm);
}

void Model::onDebugPass(const std::array<float, 16>& proj,
                        const std::array<float, 16>& view,
                        const std::array<float, 16>& model) {
    if (mShowRigidBodies) {
        auto* dbg = physicsDebug();
        if (dbg)
            dbg->onDebugPass(proj, view, model);
    }
}

void Model::enablePhysics(bool on) {
    mPhysics.enabled = on;
}

// --- Morph iteration (reads mPmx directly, no animation state needed) ---

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

void Model::syncBoneTexture() {
    if (!mBoneDirty) return;
    auto skinMatrices = BoneSkinning::computeSkinningMatrices(
        mPoseWorld, mInvBindPoseWorld, mPmx.boneCount());
    auto& bm = mMorphCtl.boneMorphs();
    if (!bm.empty())
        BoneSkinning::applyBoneMorphs(skinMatrices, mPmx.boneCount(), bm, mRenderer.modelScale());
    auto data = BoneSkinning::packBoneMatrices(skinMatrices, mPmx.boneCount());
    mRenderer.uploadBoneData(data.pixels.data(), data.pixels.size() * sizeof(float));
    mBoneDirty = false;
}

void Model::syncMorphOffsets() {
    bool idleActive = mAnimCtrl.idleEnabled() && !mAnimCtrl.isVmdPlaying();
    if (idleActive) {
        float blinkPhase = fmodf(mAnimCtrl.idleTime(), 4.0f);
        float w = 0;
        if (blinkPhase < 0.15f) {
            float t = blinkPhase / 0.15f;
            w = t < 0.5f ? t * 2.0f : (1.0f - t) * 2.0f;
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

RigidBodyRenderer* Model::physicsDebug() {
    if (!mPhysicsDebug) {
        mPhysicsDebug = std::make_unique<RigidBodyRenderer>();
        mPhysicsDebug->build(mPmx, mRenderer.modelScale(), mRenderer.modelMatrix(), &mPhysics);
        mPhysicsDebug->showRigidBody = true;
        mPhysicsDebug->showJoint = true;
        mPhysicsDebug->useBoneMatrices = false;
    }
    return mPhysicsDebug.get();
}

}  // namespace mmd
