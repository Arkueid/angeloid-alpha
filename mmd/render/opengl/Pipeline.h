#pragma once

#include "render/opengl/ModelRenderer.h"
#include "render/opengl/gpu/Shader.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// ──── Pipeline — singleton, owns all GPU shader programs ────
//
//   All shader programs are defined in effects.cfg.
//     [base] / [toon] — swappable rendering styles (T key)
//     [outline] / [rigidbody] — built-in infrastructure

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
        bool showToon = true;
        bool showRigidBodies = false;
        class RigidBodyRenderer* physicsDebug = nullptr;
        const class PhysicsWorld* physics = nullptr;
    };

    void execute(ModelRenderer& renderer, const FrameParams& p);

private:
    Pipeline() = default;

    Gpu::ShaderProgram* compile(const std::filesystem::path& shaderDir,
                                const std::string& vert, const std::string& frag);

    Gpu::ShaderProgram* mOutlineProg = nullptr;
    Gpu::ShaderProgram* mMainProg = nullptr;      // [base]
    Gpu::ShaderProgram* mMainToonProg = nullptr;  // [toon]
    Gpu::ShaderProgram* mRigidbodyProg = nullptr;  // [rigidbody]
    std::vector<std::unique_ptr<Gpu::ShaderProgram>> mPrograms;
};
