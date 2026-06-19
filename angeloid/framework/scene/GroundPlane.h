#pragma once

#include "framework/Renderable.h"
#include "framework/gpu/IGpuVertexArray.h"
#include "framework/gpu/IGpuBuffer.h"

#include <memory>

// ──── GroundPlane — infinite-feeling ground quad for shadow reception ────

class GroundPlane : public Renderable {
public:
    GroundPlane();
    ~GroundPlane();

    const char* name() const override { return "Ground"; }
    bool castShadow() const override { return false; }

    void onMainPass(const MainPassParams& mp) override;

private:
    std::unique_ptr<Gpu::IGpuVertexArray> mVao;
    std::unique_ptr<Gpu::IGpuBuffer> mVbo;  // keep-alive for VAO
};
