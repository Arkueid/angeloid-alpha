#pragma once

#include "framework/Renderable.h"
#include "framework/gpu/IGpuVertexArray.h"
#include "framework/gpu/IGpuBuffer.h"

#include <array>
#include <memory>
#include <vector>

// Generates and renders world axis (RGB) and ground grid lines in XZ plane
class WorldAxis : public Renderable {
public:
    WorldAxis();

    const char* name() const override { return "Axis"; }

    void onDebugPass(const DebugPassParams& dp) override;

    bool showAxis = true;
    bool showGrid = true;

private:
    std::unique_ptr<Gpu::IGpuVertexArray> mAxisVao;
    std::unique_ptr<Gpu::IGpuVertexArray> mGridVao;
    // Keep-alive for VBOs referenced by the VAOs
    std::vector<std::unique_ptr<Gpu::IGpuBuffer>> mBuffers;
};
