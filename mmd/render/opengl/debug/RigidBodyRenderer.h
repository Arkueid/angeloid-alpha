#pragma once

#include "pmx/PmxModel.h"
#include "render/opengl/gpu/Mesh.h"
#include "render/opengl/gpu/Shader.h"
#include "render/opengl/gpu/Texture.h"

#include <array>
#include <vector>

class PhysicsWorld;

class RigidBodyRenderer {
public:
    void build(const PmxModel& model, float modelScale = 1.0f);
    void updateFromPhysics(const PhysicsWorld& world);

    void render(Gpu::ShaderProgram& shader, const std::array<float, 16>& projection,
                const std::array<float, 16>& view, const float* modelMat = nullptr) const;

    bool showRigidBody = true;
    bool showJoint = true;
    bool useBoneMatrices = false;

private:
    Gpu::Vao mRbStatic, mRbAnimated;
    Gpu::Vao mJtStatic, mJtAnimated;
    Gpu::Vao mRbPhysics, mJtPhysics;
    // Body world transform texture (updated per-frame from physics)
    std::unique_ptr<Gpu::Texture> mBodyTex;
    int mBodyTexWidth = 64;
    int mBodyCount = 0;
    float mModelScale = 1.0f;
    float mCx = 0, mMy = 0, mCz = 0;
};
