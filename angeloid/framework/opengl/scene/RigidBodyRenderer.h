#pragma once

#include "core/pmx/PmxModel.h"
#include "framework/opengl/Renderable.h"
#include "framework/opengl/gpu/Mesh.h"
#include "framework/opengl/gpu/Shader.h"
#include "framework/opengl/gpu/Texture.h"

#include <vector>

class PhysicsWorld;

class RigidBodyRenderer : public Renderable {
public:
    void build(const PmxModel& model, float modelScale = 1.0f,
               const float* modelMat = nullptr,
               const PhysicsWorld* physicsWorld = nullptr);

    const char* name() const override { return "RigidBody"; }

    void onDebugPass(const DebugPassParams& dp) override;

    bool showRigidBody = true;
    bool showJoint = true;
    bool useBoneMatrices = false;

private:
    void updateFromPhysics();

    Gpu::Vao mRbStatic, mRbAnimated;
    Gpu::Vao mJtStatic, mJtAnimated;
    Gpu::Vao mRbPhysics, mJtPhysics;
    std::unique_ptr<Gpu::Texture> mBodyTex;
    int mBodyTexWidth = 64;
    int mBodyCount = 0;
    float mModelScale = 1.0f;
    const float* mModelMat = nullptr;
    const PhysicsWorld* mPhysicsWorld = nullptr;
    float mCx = 0, mMy = 0, mCz = 0;
};
