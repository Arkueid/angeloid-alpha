#pragma once

#include "framework/opengl/gpu/Mesh.h"
#include "framework/opengl/gpu/Shader.h"

#include <array>

// Generates and renders world axis (RGB) and ground grid lines in XZ plane
class WorldAxis {
public:
    WorldAxis();

    void render(const Gpu::ShaderProgram& shader, const std::array<float, 16>& projection,
                const std::array<float, 16>& view) const;

    bool showAxis = true;
    bool showGrid = true;

private:
    Gpu::Vao mAxisVao;
    Gpu::Vao mGridVao;
};
