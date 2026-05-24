#pragma once

#include "Gpu/Mesh.h"
#include "Gpu/Texture.h"
#include "anim/MorphController.h"
#include "pmx/PmxModel.h"
#include "anim/VpdLoader.h"

#include <array>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Gpu { class ShaderProgram; }

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
    ~ModelRenderer() = default;

    void loadModel(const PmxModel& model,
                   const std::filesystem::path& textureDir,
                   const std::filesystem::path& toonDir);

    void setupSkinning(const PmxModel& model,
                       const std::filesystem::path& vpdPath = {});

    void renderMainPass(Gpu::ShaderProgram& shader,
                        const std::array<float, 16>& projection,
                        const std::array<float, 16>& view,
                        const float* modelMat = nullptr);

    void renderSkinnedMainPass(Gpu::ShaderProgram& shader,
                               const std::array<float, 16>& projection,
                               const std::array<float, 16>& view,
                               const float* modelMat = nullptr);

    void renderOutlinePass(Gpu::ShaderProgram& shader,
                           const std::array<float, 16>& projection,
                           const std::array<float, 16>& view,
                           const float* modelMat = nullptr);

    void renderSkinnedOutlinePass(Gpu::ShaderProgram& shader,
                                  const std::array<float, 16>& projection,
                                  const std::array<float, 16>& view,
                                  const float* modelMat = nullptr);

    void applyPhysics(const PmxModel& model,
                      const std::vector<std::array<float, 16>>& physicsMats);

    void updateBoneTexture(const PmxModel& model,
                           const std::vector<std::array<float, 16>>& poseWorld,
                           const std::unordered_map<int, BoneMorphTransform>* boneMorphs = nullptr);

    void updateBoneTexture(const PmxModel& model,
                           const std::unordered_map<std::string, VpdPose>& vpdPoses,
                           const std::unordered_map<std::string,
                               std::pair<std::array<float,3>, std::array<float,4>>>& vmdTransforms,
                           const std::unordered_map<int, BoneMorphTransform>* boneMorphs = nullptr);

    const Gpu::Texture* boneTexture() const { return mBoneTexture.get(); }
    int boneTextureWidth() const { return mBoneTextureWidth; }

    bool showModel = true;
    bool showOutline = true;
    bool showToon = true;
    bool useSkinning = false;

    void renderMorphMainPass(Gpu::ShaderProgram& shader,
                             const std::array<float, 16>& projection,
                             const std::array<float, 16>& view,
                             const float* modelMat = nullptr);

    void renderMorphOutlinePass(Gpu::ShaderProgram& shader,
                                const std::array<float, 16>& projection,
                                const std::array<float, 16>& view,
                                const float* modelMat = nullptr);

    Gpu::VboWrapper* morphVbo() const { return mMorphVboW.get(); }
    Gpu::VboWrapper* uvMorphVbo() const { return mUvMorphVboW.get(); }
    float modelScale() const { return mScale; }
    void setMaterialOverride(int idx, const MatMorphOverride& o) { mMatOverride[idx] = o; }
    void clearMaterialOverrides() { mMatOverride.clear(); }
    const MatMorphOverride* getMaterialOverride(int idx) const {
        auto it = mMatOverride.find(idx); return it != mMatOverride.end() ? &it->second : nullptr;
    }
    const std::vector<MaterialBatch>& materialBatches() const { return mMaterialBatches; }

    const PmxModel* model() const { return mModel; }

private:
    void loadTextures(const std::filesystem::path& textureDir,
                      const std::filesystem::path& toonDir);
    void buildMaterialBatches(const PmxModel& model);

    const PmxModel* mModel = nullptr;

    // Vertex data
    std::vector<float> mVertices; // interleaved: [px,py,pz, nx,ny,nz, u,v] x N
    std::vector<int32_t> mIndices;

    // VAOs
    Gpu::Vao mModelVao;
    Gpu::Vao mToonVao;
    Gpu::Vao mOutlineVao;

    // Textures
    std::vector<std::unique_ptr<Gpu::Texture>> mTextures;
    std::unique_ptr<Gpu::Texture> mDummyTexture;
    std::unique_ptr<Gpu::Texture> mDefaultToon;

    // Material batches
    std::vector<MaterialBatch> mMaterialBatches;

    // Per-material data (indexed by material_index)
    std::vector<Vec3> mMaterialColor;
    std::vector<MaterialEdge> mMaterialEdge;
    std::vector<MaterialSpecular> mMaterialSpecular;
    std::vector<Vec3> mMaterialAmbient;
    std::vector<MaterialSphere> mMaterialSphere;
    std::vector<MaterialToon> mMaterialToon;

    // Model bounds
    Vec3 mCenter;
    float mMinY = 0;
    float mScale = 1;

    // Bone skinning
    std::unique_ptr<Gpu::Texture> mBoneTexture;
    int mBoneTextureWidth = 0;
    Gpu::Vao mSkinnedVao;
    Gpu::Vao mSkinnedVaoNoToon;
    Gpu::Vao mSkinnedOutlineVao;
    Gpu::Vao mMorphVao;
    Gpu::Vao mMorphVaoNoToon;
    Gpu::Vao mMorphOutlineVao;
    std::unique_ptr<Gpu::VboWrapper> mMorphVboW;
    std::unique_ptr<Gpu::VboWrapper> mUvMorphVboW;
    std::unordered_map<int, MatMorphOverride> mMatOverride;
};
