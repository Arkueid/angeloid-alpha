#pragma once

#include "Gpu/Mesh.h"
#include "pmx/PmxModel.h"

namespace Gpu { class ShaderProgram; }

// Generates and renders rigid body and joint wireframes
class PhysicsDebug {
public:
    void build(const PmxModel& model, float modelScale = 1.0f);

    void render(Gpu::ShaderProgram& shader,
                const std::array<float, 16>& projection,
                const std::array<float, 16>& view,
                const float* modelMat = nullptr) const;

    bool showRigidBody = true;
    bool showJoint = true;
    bool useBoneMatrices = false; // K toggles this

private:
    Gpu::Vao mRbStatic, mRbAnimated;
    Gpu::Vao mJtStatic, mJtAnimated;
};
