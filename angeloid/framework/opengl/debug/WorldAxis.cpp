#include "framework/opengl/debug/WorldAxis.h"

#include "framework/opengl/ShaderStandard.h"

#include <glad/glad.h>
#include <vector>

// 6 floats per vertex: px,py,pz, cr,cg,cb
static const int sStride = 6;

WorldAxis::WorldAxis() {
    // --- Axis lines (X=red, Y=green, Z=blue) ---
    const float axisLen = 5.0f;
    float axisVerts[] = {
        0, 0,       0, 1, 0, 0, axisLen, 0, 0, 1, 0, 0, 0, 0, 0,       0, 1, 0,
        0, axisLen, 0, 0, 1, 0, 0,       0, 0, 0, 0, 1, 0, 0, axisLen, 0, 0, 1,
    };

    mAxisVao.bind();
    GLuint axisVbo;
    glGenBuffers(1, &axisVbo);
    glBindBuffer(GL_ARRAY_BUFFER, axisVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVerts), axisVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sStride * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sStride * sizeof(float),
                          (void*)(3 * sizeof(float)));
    mAxisVao.vbos.push_back(axisVbo);
    mAxisVao.vertexCount = sizeof(axisVerts) / (sStride * sizeof(float));
    Gpu::Vao::unbind();

    // Ground grid on the XZ plane (Y=0), rendered as intersecting lines.
    // The grid is axis-aligned and renders without depth test so it's always visible
    // behind the model, providing spatial reference for the viewer.
    const float gridSize = 200.0f;
    const int gridDivs = 50;
    const float step = gridSize / gridDivs;
    const int halfDivs = gridDivs / 2;  // 12
    const int lines = gridDivs + 1;     // 26 lines per axis

    std::vector<float> gridVerts;
    gridVerts.reserve(lines * 2 * 2 * sStride);

    for (int i = -halfDivs; i <= halfDivs; ++i) {
        float v = i * step;
        // Line along Z at fixed X
        gridVerts.insert(gridVerts.end(), {v, 0, -gridSize / 2, 0.5f, 0.5f, 0.5f});
        gridVerts.insert(gridVerts.end(), {v, 0, gridSize / 2, 0.5f, 0.5f, 0.5f});
        // Line along X at fixed Z
        gridVerts.insert(gridVerts.end(), {-gridSize / 2, 0, v, 0.5f, 0.5f, 0.5f});
        gridVerts.insert(gridVerts.end(), {gridSize / 2, 0, v, 0.5f, 0.5f, 0.5f});
    }

    mGridVao.bind();
    GLuint gridVbo;
    glGenBuffers(1, &gridVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gridVbo);
    glBufferData(GL_ARRAY_BUFFER, gridVerts.size() * sizeof(float), gridVerts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sStride * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sStride * sizeof(float),
                          (void*)(3 * sizeof(float)));
    mGridVao.vbos.push_back(gridVbo);
    mGridVao.vertexCount = (int)gridVerts.size() / sStride;
    Gpu::Vao::unbind();
}

void WorldAxis::render(const Gpu::ShaderProgram& shader, const std::array<float, 16>& projection,
                       const std::array<float, 16>& view) const {
    float identity[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    shader.use();
    shader.setMat4(U_PROJ_MAT, projection.data());
    shader.setMat4(U_VIEW_MAT, view.data());
    shader.setMat4(U_MODEL_MAT, identity);

    // Polygon offset prevents z-fighting with the ground plane at Y=0
    // while still letting the model occlude axis lines behind it.
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    if (showGrid) {
        mGridVao.render(GL_LINES);
    }
    if (showAxis) {
        glLineWidth(2.0f);
        mAxisVao.render(GL_LINES);
        glLineWidth(1.0f);
    }
    glDisable(GL_POLYGON_OFFSET_LINE);
}
