#pragma once

#include "framework/opengl/ModelRenderer.h"
#include "framework/opengl/RenderTarget.h"
#include "framework/opengl/gpu/Shader.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class WorldAxis;

// ──── Pipeline — singleton, owns all GPU shader programs ────
//
//   All shader programs are defined in effects.cfg.
//     [base] / [toon] — swappable rendering styles (T key)
//     [shadow] / [outline] / [rigidbody] / [ground] / [axis] — built-in infrastructure

class Pipeline {
public:
    static Pipeline& instance();

    void init(const std::filesystem::path& effectsCfg,
              const std::filesystem::path& shaderDir);
    void clear();

    struct FrameParams {
        const std::array<float, 16>* proj = nullptr;
        const std::array<float, 16>* view = nullptr;
        const float* modelMat = nullptr;
        float camPosX = 0, camPosY = 0, camPosZ = 10;
        float lightDirX = 0, lightDirY = 0.5f, lightDirZ = 1.0f;

        const std::array<float, 16>* lightViewProj = nullptr;

        bool showToon = true;
        bool showRigidBodies = false;
        bool showGround = true;
        class RigidBodyRenderer* physicsDebug = nullptr;
        const class PhysicsWorld* physics = nullptr;
    };

    void execute(ModelRenderer& renderer, const FrameParams& p);

    void setWorldAxis(WorldAxis* wa) { mWorldAxis = wa; }

    void resizeViewport(int w, int h);

private:
    Pipeline() = default;

    Gpu::ShaderProgram* compile(const std::filesystem::path& shaderDir,
                                const std::string& vert, const std::string& frag);

    void renderShadowPass(ModelRenderer& renderer, const FrameParams& p);
    void renderGroundPass(const std::array<float, 16>& proj,
                          const std::array<float, 16>& view,
                          bool hasShadow);
    void renderAxisPass(const std::array<float, 16>& proj,
                        const std::array<float, 16>& view);
    void renderOutlinePass(ModelRenderer& renderer,
                           const std::array<float, 16>& proj,
                           const std::array<float, 16>& view,
                           const float* modelMat);
    void renderMainPass(ModelRenderer& renderer, const FrameParams& p,
                        const std::array<float, 16>& proj,
                        const std::array<float, 16>& view,
                        const float* modelMat);
    void renderPhysicsPass(const FrameParams& p,
                           const std::array<float, 16>& proj,
                           const std::array<float, 16>& view,
                           const float* modelMat);

    Gpu::ShaderProgram* mShadowProg = nullptr;
    Gpu::ShaderProgram* mOutlineProg = nullptr;
    Gpu::ShaderProgram* mMainProg = nullptr;
    Gpu::ShaderProgram* mMainToonProg = nullptr;
    Gpu::ShaderProgram* mRigidbodyProg = nullptr;
    Gpu::ShaderProgram* mGroundProg = nullptr;
    Gpu::ShaderProgram* mAxisProg = nullptr;

    GLuint mGroundVao = 0;
    GLuint mGroundVbo = 0;
    int mGroundVertCount = 0;

    WorldAxis* mWorldAxis = nullptr;

    RenderTarget mShadowMap;
    std::array<float, 16> mLightViewProj{};
    int mViewportW = 0, mViewportH = 0;

    std::vector<std::unique_ptr<Gpu::ShaderProgram>> mPrograms;
};
