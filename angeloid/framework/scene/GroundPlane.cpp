#include "framework/scene/GroundPlane.h"

#include "framework/ShaderManager.h"
#include "framework/ShaderStandard.h"
#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/Types.h"

GroundPlane::GroundPlane() {
    // 200×200 quad at Y=0
    float groundSize = 100.0f;
    float verts[] = {
        -groundSize, 0, -groundSize,
         groundSize, 0, -groundSize,
         groundSize, 0,  groundSize,
        -groundSize, 0, -groundSize,
         groundSize, 0,  groundSize,
        -groundSize, 0,  groundSize,
    };

    auto* dev = Gpu::device();
    mVbo = dev->createVertexBuffer(verts, sizeof(verts), Gpu::BufferUsage::Static);

    std::vector<Gpu::VertexAttribute> attrs = {
        {0, 3, Gpu::DataType::Float, 3 * (int)sizeof(float), 0},
    };
    std::vector<Gpu::IGpuBuffer*> bufs = {mVbo.get()};
    mVao = dev->createVertexArray(attrs, bufs, nullptr, Gpu::IndexType::UInt32, 6, 0);
}

void GroundPlane::onMainPass(const MainPassParams& mp) {
    auto* shader = mp.shaders->ground();
    if (!shader) return;

    shader->use();
    shader->setMat4(U_PROJ_MAT, mp.proj.data());
    shader->setMat4(U_VIEW_MAT, mp.view.data());
    if (mp.hasShadow) {
        shader->setInt(U_SHADOW_MAP, 5);
        shader->setMat4(U_LIGHT_VIEW_PROJ, mp.lightViewProj.data());
        shader->setInt(U_HAS_SHADOW, 1);
    } else {
        shader->setInt(U_HAS_SHADOW, 0);
    }
    mVao->draw(Gpu::PrimitiveType::Triangles, 6);
}

GroundPlane::~GroundPlane() = default;
