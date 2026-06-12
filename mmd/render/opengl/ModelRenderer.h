#pragma once

#include "anim/MorphController.h"
#include "anim/VpdLoader.h"
#include "pmx/PmxModel.h"
#include "render/opengl/gpu/Mesh.h"
#include "render/opengl/gpu/Texture.h"

#include <array>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Gpu {
class ShaderProgram;
}

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

    const Gpu::Texture* boneTexture() const { return mBoneTexture.get(); }
    int boneTextureWidth() const { return mBoneTextureWidth; }

    bool showModel = true;
    bool showOutline = true;
    bool showToon = true;

    // Depth-only pass for shadow map generation (no textures, no materials)
    void renderDepthPass(Gpu::ShaderProgram& shader, const std::array<float, 16>& lightViewProj,
                         const float* modelMat = nullptr);

    void renderMorphMainPass(Gpu::ShaderProgram& shader, const std::array<float, 16>& projection,
                             const std::array<float, 16>& view, const float* modelMat = nullptr);

    void renderMorphOutlinePass(Gpu::ShaderProgram& shader, const std::array<float, 16>& projection,
                                const std::array<float, 16>& view, const float* modelMat = nullptr);

    Gpu::VboWrapper* morphVbo() const  { return mMorphVboW.get(); }
    Gpu::VboWrapper* uvMorphVbo() const { return mUvMorphVboW.get(); }
    float modelScale() const            { return mScale; }
    const float* modelMatrix() const    { return mModelMat.data(); }
    const PmxModel* model() const       { return mModel; }

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
    std::vector<std::unique_ptr<Gpu::Texture>> mTextures;
    std::unique_ptr<Gpu::Texture> mDummyTexture;

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
    std::unique_ptr<Gpu::Texture> mBoneTexture;
    int mBoneTextureWidth = 0;
    Gpu::Vao mMorphVao;
    Gpu::Vao mMorphVaoNoToon;
    Gpu::Vao mMorphOutlineVao;
    std::unique_ptr<Gpu::VboWrapper> mMorphVboW;
    std::unique_ptr<Gpu::VboWrapper> mUvMorphVboW;
    std::unordered_map<int, MatMorphOverride> mMatOverride;

};
