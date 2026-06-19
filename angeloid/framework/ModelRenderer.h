#pragma once

#include "core/anim/MorphController.h"
#include "core/anim/VpdLoader.h"
#include "core/pmx/PmxModel.h"
#include "framework/gpu/IGpuBuffer.h"
#include "framework/gpu/IGpuTexture.h"
#include "framework/gpu/IGpuShader.h"
#include "framework/gpu/IGpuVertexArray.h"

#include <array>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

struct MaterialBatch {
    int first = 0;
    int count = 0;
    int textureIndex = -1;
    int materialIndex = 0;
    bool hasEdge = false;
};

struct MaterialEdge {
    Vec4 color = {0, 0, 0, 1};
    float size = 0;
};

struct MaterialSpecular {
    Vec3 color;
    float factor = 1;
};

struct MaterialToon {
    int textureIndex = -1;
    int sharingFlag = 0;
};

struct MaterialSphere {
    int textureIndex = -1;
    int mode = 0;
};

class ModelRenderer {
public:
    ModelRenderer();
    ~ModelRenderer();

    void loadModel(const PmxModel& model, const std::filesystem::path& textureDir);
    void setupSkinning(const PmxModel& model, const std::filesystem::path& vpdPath = {});

    void uploadBoneData(const void* data, size_t bytes);

    const Gpu::IGpuTexture* boneTexture() const { return mBoneTexture.get(); }
    int boneTextureWidth() const { return mBoneTextureWidth; }

    bool showModel = true;
    bool showOutline = true;
    bool showToon = true;

    // Depth-only pass for shadow map generation (no textures, no materials)
    void renderDepthPass(Gpu::IGpuShader& shader,
                         const std::array<float, 16>& lightViewProj,
                         const float* modelMat = nullptr);

    void renderMorphMainPass(Gpu::IGpuShader& shader,
                             const std::array<float, 16>& proj,
                             const std::array<float, 16>& view,
                             const float* modelMat = nullptr);

    void renderMorphOutlinePass(Gpu::IGpuShader& shader,
                                const std::array<float, 16>& proj,
                                const std::array<float, 16>& view,
                                const float* modelMat = nullptr);

    Gpu::IGpuBuffer* morphVbo() const   { return mMorphVboBuffer.get(); }
    Gpu::IGpuBuffer* uvMorphVbo() const { return mUvMorphVboBuffer.get(); }
    float modelScale() const            { return mScale; }
    const float* modelMatrix() const    { return mModelMat.data(); }
    const PmxModel* model() const       { return mModel; }

    // PMX-space vertex bounds (set by loadModel)
    Vec3 mPmxMin{}, mPmxMax{};
    // Compute world-space AABB by transforming PMX bounds by model matrix
    void worldAABB(Vec3& outMin, Vec3& outMax) const;

    void setMaterialOverride(int idx, const MatMorphOverride& o) { mMatOverride[idx] = o; }
    void clearMaterialOverrides() { mMatOverride.clear(); }
    const MatMorphOverride* getMaterialOverride(int idx) const {
        auto it = mMatOverride.find(idx);
        return it != mMatOverride.end() ? &it->second : nullptr;
    }

private:
    void loadTextures(const std::filesystem::path& textureDir);
    void buildMaterialBatches(const PmxModel& model);

    const PmxModel* mModel = nullptr;

    std::vector<int32_t> mIndices;

    // Textures
    std::vector<std::unique_ptr<Gpu::IGpuTexture>> mTextures;
    std::unique_ptr<Gpu::IGpuTexture> mDummyTexture;

    // Material batches
    std::vector<MaterialBatch> mMaterialBatches;
    std::vector<Vec3> mMaterialColor;
    std::vector<MaterialEdge> mMaterialEdge;
    std::vector<MaterialSpecular> mMaterialSpecular;
    std::vector<Vec3> mMaterialAmbient;
    std::vector<MaterialSphere> mMaterialSphere;
    std::vector<MaterialToon> mMaterialToon;

    // Model bounds & display transform
    Vec3 mCenter;
    float mMinY = 0;
    float mScale = 1;
    std::array<float, 16> mModelMat = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1};

    // Bone skinning
    std::unique_ptr<Gpu::IGpuTexture> mBoneTexture;
    int mBoneTextureWidth = 0;
    std::unique_ptr<Gpu::IGpuVertexArray> mMorphVao;
    std::unique_ptr<Gpu::IGpuBuffer> mMorphVboBuffer;
    std::unique_ptr<Gpu::IGpuBuffer> mUvMorphVboBuffer;
    std::unique_ptr<Gpu::IGpuBuffer> mIndexBuffer;
    // Keep-alive for base vertex buffers referenced by mMorphVao
    std::vector<std::unique_ptr<Gpu::IGpuBuffer>> mBaseBuffers;
    int mIndexCount = 0;
    std::unordered_map<int, MatMorphOverride> mMatOverride;
};
