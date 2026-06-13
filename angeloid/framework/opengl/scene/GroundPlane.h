#pragma once

#include "framework/opengl/Renderable.h"

#include <glad/glad.h>

// ──── GroundPlane — infinite-feeling ground quad for shadow reception ────

class GroundPlane : public Renderable {
public:
    GroundPlane();
    ~GroundPlane();

    const char* name() const override { return "Ground"; }
    bool castShadow() const override { return false; }

    void onMainPass(const std::array<float, 16>& proj,
                    const std::array<float, 16>& view,
                    const std::array<float, 16>& model,
                    const std::array<float, 16>& lightViewProj,
                    bool hasShadow) override;

private:
    GLuint mVao = 0;
    GLuint mVbo = 0;
};
