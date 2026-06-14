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

    void onMainPass(const MainPassParams& mp) override;

private:
    GLuint mVao = 0;
    GLuint mVbo = 0;
};
