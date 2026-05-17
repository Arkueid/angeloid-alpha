#pragma once

#include "Gpu/Mesh.h"
#include "Gpu/Texture.h"
#include "Gpu/Shader.h"
#include "pmx/PmxModel.h"

#include <array>
#include <vector>

class PhysicsWorld;

class PhysicsDebug {
public:
    void build(const PmxModel& model, float modelScale = 1.0f);
    void updateFromPhysics(const PhysicsWorld& world);

    void render(Gpu::ShaderProgram& shader,
                const std::array<float, 16>& projection,
                const std::array<float, 16>& view,
                const float* modelMat = nullptr) const;

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
};
