#include "render/opengl/Pipeline.h"

#include <GL/glew.h>

#include "anim/PhysicsWorld.h"
#include "render/opengl/RenderContext.h"
#include "render/opengl/ShaderStandard.h"
#include "render/opengl/debug/RigidBodyRenderer.h"
#include "util/CfgParser.h"
#include "util/Log.h"

namespace fs = std::filesystem;

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

    if (auto* c = get("outline"))
        mOutlineProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("base"))
        mMainProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("toon"))
        mMainToonProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    if (auto* c = get("rigidbody"))
        mRigidbodyProg = compile(shaderDir, (*c)["vert"], (*c)["frag"]);

    MMD_INFO("PIPELINE", "Initialized (%zu programs)", mPrograms.size());
}

void Pipeline::clear() {
    mPrograms.clear();
}

void Pipeline::execute(ModelRenderer& renderer, const FrameParams& p) {
    if (!p.proj || !p.view) return;

    glFrontFace(GL_CW);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto& proj     = *p.proj;
    const auto& view     = *p.view;
    const auto* modelMat = p.modelMat;

    if (renderer.showOutline && mOutlineProg)
        renderer.renderMorphOutlinePass(*mOutlineProg, proj, view, modelMat);

    if (renderer.showModel) {
        auto* s = p.showToon ? mMainToonProg : mMainProg;
        if (s) {
            if (p.showToon) {
                s->use();
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
    }

    if (p.showRigidBodies && p.physicsDebug && p.physics && mRigidbodyProg) {
        if (p.physicsDebug->showRigidBody) {
            p.physicsDebug->updateFromPhysics(*p.physics);
            p.physicsDebug->render(*mRigidbodyProg, proj, view, modelMat);
        }
    }
}
