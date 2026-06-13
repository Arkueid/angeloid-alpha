#include "framework/opengl/Pipeline.h"

#include <glad/glad.h>

#include "core/anim/PhysicsWorld.h"
#include "framework/opengl/RenderContext.h"
#include "framework/opengl/ShaderStandard.h"
#include "framework/opengl/debug/RigidBodyRenderer.h"
#include "framework/opengl/debug/WorldAxis.h"
#include "framework/util/CfgParser.h"
#include "core/util/Log.h"

namespace fs = std::filesystem;

static constexpr int kShadowMapSize = 4096;

Pipeline& Pipeline::instance() {
    static Pipeline p;
    return p;
}

Gpu::ShaderProgram* Pipeline::compile(const fs::path& shaderDir,
                                       const std::string& vert,
                                       const std::string& frag) {
    auto vs = Gpu::ShaderProgram::readFile(shaderDir / vert);
    auto fs = Gpu::ShaderProgram::readFile(shaderDir / frag);
    if (vs.empty() || fs.empty()) {
        MMD_WARN("PIPELINE", "Failed to read %s / %s", vert.c_str(), frag.c_str());
        return nullptr;
    }
    auto prog = std::make_unique<Gpu::ShaderProgram>(vs, fs);
    auto* ptr = prog.get();
    mPrograms.push_back(std::move(prog));
    return ptr;
}

void Pipeline::init(const fs::path& effectsCfg, const fs::path& shaderDir) {
    auto sections = parseCfgSections(effectsCfg);

    auto get = [&](const char* name) -> std::unordered_map<std::string, std::string>* {
        auto it = sections.find(name);
        return it != sections.end() ? &it->second : nullptr;
    };

    if (auto* c = get("shadow"))
        mShadowProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("outline"))
        mOutlineProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("base"))
        mMainProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("toon"))
        mMainToonProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("rigidbody"))
        mRigidbodyProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("ground"))
        mGroundProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("axis"))
        mAxisProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    // Create ground plane VAO (large quad on XZ plane)
    if (mGroundProg) {
        float groundSize = 100.0f;
        float verts[] = {
            -groundSize, 0, -groundSize,
             groundSize, 0, -groundSize,
             groundSize, 0,  groundSize,
            -groundSize, 0, -groundSize,
             groundSize, 0,  groundSize,
            -groundSize, 0,  groundSize,
        };
        glGenVertexArrays(1, &mGroundVao);
        glBindVertexArray(mGroundVao);
        glGenBuffers(1, &mGroundVbo);
        glBindBuffer(GL_ARRAY_BUFFER, mGroundVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
        mGroundVertCount = 6;
    }

    MMD_INFO("PIPELINE", "Initialized (%zu programs)", mPrograms.size());
}

void Pipeline::clear() {
    if (mGroundVao) {
        glDeleteVertexArrays(1, &mGroundVao);
        mGroundVao = 0;
    }
    if (mGroundVbo) {
        glDeleteBuffers(1, &mGroundVbo);
        mGroundVbo = 0;
    }
    mPrograms.clear();
    mShadowProg = nullptr;
    mOutlineProg = nullptr;
    mMainProg = nullptr;
    mMainToonProg = nullptr;
    mRigidbodyProg = nullptr;
    mGroundProg = nullptr;
    mAxisProg = nullptr;
}

void Pipeline::resizeViewport(int w, int h) {
    mViewportW = w;
    mViewportH = h;
    if (mShadowProg)
        mShadowMap.resize(kShadowMapSize, kShadowMapSize, false, true);  // depth-only
}

void Pipeline::renderShadowPass(ModelRenderer& renderer, const FrameParams& p) {
    if (!mShadowProg || !p.lightViewProj) return;

    mLightViewProj = *p.lightViewProj;

    GLint prevDepthFunc;
    glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);

    mShadowMap.bind();
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LESS);

    renderer.renderDepthPass(*mShadowProg, *p.lightViewProj, p.modelMat);

    glDepthFunc(prevDepthFunc);
}

void Pipeline::execute(ModelRenderer& renderer, const FrameParams& p) {
    if (!p.proj || !p.view) return;

    glFrontFace(GL_CW);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderShadowPass(renderer, p);
    RenderTarget::bindScreen(mViewportW, mViewportH);

    const auto& proj = *p.proj;
    const auto& view = *p.view;

    renderGroundPass(proj, view, /*hasShadow=*/p.lightViewProj != nullptr);
    renderAxisPass(proj, view);
    renderOutlinePass(renderer, proj, view, p.modelMat);
    renderMainPass(renderer, p, proj, view, p.modelMat);
    renderPhysicsPass(p, proj, view, p.modelMat);
}

void Pipeline::renderGroundPass(const std::array<float, 16>& proj,
                                const std::array<float, 16>& view,
                                bool hasShadow) {
    if (!mGroundProg) return;

    mGroundProg->use();
    mGroundProg->setMat4(U_PROJ_MAT, proj.data());
    mGroundProg->setMat4(U_VIEW_MAT, view.data());
    if (mShadowProg && hasShadow) {
        mGroundProg->setInt("u_shadowMap", 5);
        mGroundProg->setMat4("u_lightViewProj", mLightViewProj.data());
        mGroundProg->setInt("u_hasShadow", 1);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, mShadowMap.depthTex());
    } else {
        mGroundProg->setInt("u_hasShadow", 0);
    }
    glBindVertexArray(mGroundVao);
    glDrawArrays(GL_TRIANGLES, 0, mGroundVertCount);
    glBindVertexArray(0);
}

void Pipeline::renderAxisPass(const std::array<float, 16>& proj,
                              const std::array<float, 16>& view) {
    if (mWorldAxis && mAxisProg)
        mWorldAxis->render(*mAxisProg, proj, view);
}

void Pipeline::renderOutlinePass(ModelRenderer& renderer,
                                  const std::array<float, 16>& proj,
                                  const std::array<float, 16>& view,
                                  const float* modelMat) {
    if (renderer.showOutline && mOutlineProg)
        renderer.renderMorphOutlinePass(*mOutlineProg, proj, view, modelMat);
}

void Pipeline::renderMainPass(ModelRenderer& renderer, const FrameParams& p,
                               const std::array<float, 16>& proj,
                               const std::array<float, 16>& view,
                               const float* modelMat) {
    if (!renderer.showModel) return;

    auto* s = p.showToon ? mMainToonProg : mMainProg;
    if (!s) return;

    s->use();

    if (mShadowProg && p.lightViewProj) {
        s->setInt("u_shadowMap", 5);
        s->setMat4("u_lightViewProj", p.lightViewProj->data());
        s->setInt("u_hasShadow", 1);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, mShadowMap.depthTex());
    } else {
        s->setInt("u_hasShadow", 0);
    }

    s->setVec3(U_LIGHT_DIR, p.lightDirX, p.lightDirY, p.lightDirZ);

    if (p.showToon) {
        s->setVec3(U_CAMERA_POS, p.camPosX, p.camPosY, p.camPosZ);
        s->setFloat(U_SHADOW_THRESH, 0.0f);
        s->setFloat(U_RIM_POWER, 4.0f);
        s->setVec3(U_RIM_COLOR, 1.0f, 1.0f, 1.0f);
        s->setInt("u_gradientMap", TEX_UNIT_GRADIENT);
        auto& ctx = mmd::RenderContext::instance();
        glActiveTexture(GL_TEXTURE0 + TEX_UNIT_GRADIENT);
        glBindTexture(GL_TEXTURE_2D, ctx.gradientTexture()->id);
    }

    renderer.renderMorphMainPass(*s, proj, view, modelMat);
}

void Pipeline::renderPhysicsPass(const FrameParams& p,
                                  const std::array<float, 16>& proj,
                                  const std::array<float, 16>& view,
                                  const float* modelMat) {
    if (!p.showRigidBodies || !p.physicsDebug || !p.physics || !mRigidbodyProg)
        return;
    if (!p.physicsDebug->showRigidBody)
        return;

    p.physicsDebug->updateFromPhysics(*p.physics);
    p.physicsDebug->render(*mRigidbodyProg, proj, view, modelMat);
}
