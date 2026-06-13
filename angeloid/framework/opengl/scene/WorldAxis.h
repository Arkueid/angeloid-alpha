#pragma once

#include "framework/opengl/Renderable.h"
#include "framework/opengl/gpu/Mesh.h"
#include "framework/opengl/gpu/Shader.h"

#include <array>

// Generates and renders world axis (RGB) and ground grid lines in XZ plane
class WorldAxis : public Renderable {
public:
    WorldAxis();

    const char* name() const override { return "Axis"; }

    void onDebugPass(const std::array<float, 16>& proj,
                     const std::array<float, 16>& view,
                     const std::array<float, 16>& model) override;

    bool showAxis = true;
    bool showGrid = true;

private:
    Gpu::Vao mAxisVao;
    Gpu::Vao mGridVao;
};
