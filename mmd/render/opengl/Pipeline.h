#pragma once

#include "render/opengl/ModelRenderer.h"
#include "render/opengl/RenderTarget.h"
#include "render/opengl/gpu/Shader.h"

#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// ──── Pipeline — singleton, owns all GPU shader programs ────
//
//   All shader programs are defined in effects.cfg.
//     [base] / [toon] — swappable rendering styles (T key)
//     [shadow] / [outline] / [rigidbody] — built-in infrastructure

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

        // Shadow map: light-space view-projection matrix
        const std::array<float, 16>* lightViewProj = nullptr;

        bool showToon = true;
        bool showRigidBodies = false;
        class RigidBodyRenderer* physicsDebug = nullptr;
        const class PhysicsWorld* physics = nullptr;
    };

    void execute(ModelRenderer& renderer, const FrameParams& p);

    // Call when window resizes
    void resizeViewport(int w, int h);

private:
    Pipeline() = default;

    Gpu::ShaderProgram* compile(const std::filesystem::path& shaderDir,
                                const std::string& vert, const std::string& frag);

    void renderShadowPass(ModelRenderer& renderer, const FrameParams& p);

    Gpu::ShaderProgram* mShadowProg = nullptr;
    Gpu::ShaderProgram* mOutlineProg = nullptr;
    Gpu::ShaderProgram* mMainProg = nullptr;      // [base]
    Gpu::ShaderProgram* mMainToonProg = nullptr;  // [toon]
    Gpu::ShaderProgram* mRigidbodyProg = nullptr;  // [rigidbody]
    Gpu::ShaderProgram* mGroundProg = nullptr;      // [ground]

    GLuint mGroundVao = 0;
    int mGroundVertCount = 0;

    RenderTarget mShadowMap;
    int mViewportW = 0, mViewportH = 0;

    std::vector<std::unique_ptr<Gpu::ShaderProgram>> mPrograms;
};
