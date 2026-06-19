#pragma once

#include "core/pmx/PmxModel.h"
#include "framework/Renderable.h"
#include "framework/gpu/IGpuVertexArray.h"
#include "framework/gpu/IGpuTexture.h"
#include "framework/gpu/IGpuBuffer.h"

#include <memory>
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

    std::unique_ptr<Gpu::IGpuVertexArray> mRbStatic, mRbAnimated;
    std::unique_ptr<Gpu::IGpuVertexArray> mJtStatic, mJtAnimated;
    std::unique_ptr<Gpu::IGpuVertexArray> mRbPhysics, mJtPhysics;
    std::unique_ptr<Gpu::IGpuTexture> mBodyTex;
    int mBodyTexWidth = 64;
    int mBodyCount = 0;
    float mModelScale = 1.0f;
    const float* mModelMat = nullptr;
    const PhysicsWorld* mPhysicsWorld = nullptr;
    float mCx = 0, mMy = 0, mCz = 0;

    // Keep-alive for buffers referenced by VAOs
    std::vector<std::unique_ptr<Gpu::IGpuBuffer>> mBuffers;
};
