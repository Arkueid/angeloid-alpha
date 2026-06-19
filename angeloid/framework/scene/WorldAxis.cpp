#include "framework/scene/WorldAxis.h"

#include "framework/ShaderManager.h"
#include "framework/ShaderStandard.h"
#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/Types.h"

#include <vector>

// 6 floats per vertex: px,py,pz, cr,cg,cb
static const int sStride = 6;

WorldAxis::WorldAxis() {
    auto* dev = Gpu::device();

    // --- Axis lines (X=red, Y=green, Z=blue) ---
    const float axisLen = 5.0f;
    float axisVerts[] = {
        0, 0,       0, 1, 0, 0, axisLen, 0, 0, 1, 0, 0, 0, 0, 0,       0, 1, 0,
        0, axisLen, 0, 0, 1, 0, 0,       0, 0, 0, 0, 1, 0, 0, axisLen, 0, 0, 1,
    };

    // --- Ground grid on the XZ plane (Y=0) ---
    const float gridSize = 200.0f;
    const int gridDivs = 50;
    const float step = gridSize / gridDivs;
    const int halfDivs = gridDivs / 2;

    std::vector<float> gridVerts;
    gridVerts.reserve((gridDivs + 1) * 2 * 2 * sStride);

    for (int i = -halfDivs; i <= halfDivs; ++i) {
        float v = i * step;
        gridVerts.insert(gridVerts.end(), {v, 0, -gridSize / 2, 0.5f, 0.5f, 0.5f});
        gridVerts.insert(gridVerts.end(), {v, 0, gridSize / 2, 0.5f, 0.5f, 0.5f});
        gridVerts.insert(gridVerts.end(), {-gridSize / 2, 0, v, 0.5f, 0.5f, 0.5f});
        gridVerts.insert(gridVerts.end(), {gridSize / 2, 0, v, 0.5f, 0.5f, 0.5f});
    }

    // Attribute layout: position(3f) + color(3f), interleaved
    std::vector<Gpu::VertexAttribute> attrs = {
        {0, 3, Gpu::DataType::Float, sStride * (int)sizeof(float), 0},
        {1, 3, Gpu::DataType::Float, sStride * (int)sizeof(float), 3 * (int)sizeof(float)},
    };

    // Axis VAO — same VBO for both interleaved attributes
    auto axisVbo = dev->createVertexBuffer(axisVerts, sizeof(axisVerts), Gpu::BufferUsage::Static);
    int axisVc = (int)(sizeof(axisVerts) / (sStride * sizeof(float)));
    auto* axisVboPtr = axisVbo.get();
    mBuffers.push_back(std::move(axisVbo));
    mAxisVao = dev->createVertexArray(attrs, {axisVboPtr, axisVboPtr}, nullptr,
                                       Gpu::IndexType::UInt32, axisVc, 0);

    // Grid VAO — same VBO for both interleaved attributes
    auto gridVbo = dev->createVertexBuffer(gridVerts.data(), gridVerts.size() * sizeof(float),
                                            Gpu::BufferUsage::Static);
    int gridVc = (int)gridVerts.size() / sStride;
    auto* gridVboPtr = gridVbo.get();
    mBuffers.push_back(std::move(gridVbo));
    mGridVao = dev->createVertexArray(attrs, {gridVboPtr, gridVboPtr}, nullptr,
                                       Gpu::IndexType::UInt32, gridVc, 0);
}

void WorldAxis::onDebugPass(const DebugPassParams& dp) {
    auto* shader = ShaderManager::instance().axis();
    if (!shader) return;

    auto* dev = Gpu::device();

    float identity[] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    shader->use();
    shader->setMat4(U_PROJ_MAT, dp.proj.data());
    shader->setMat4(U_VIEW_MAT, dp.view.data());
    shader->setMat4(U_MODEL_MAT, identity);

    // Polygon offset prevents z-fighting with the ground plane at Y=0
    dev->setPolygonOffset(-1.0f, -1.0f);
    if (showGrid) {
        mGridVao->draw(Gpu::PrimitiveType::Lines);
    }
    if (showAxis) {
        dev->setLineWidth(2.0f);
        mAxisVao->draw(Gpu::PrimitiveType::Lines);
        dev->setLineWidth(1.0f);
    }
    dev->setPolygonOffset(0.0f, 0.0f);  // disable
}
