#include "framework/ModelRenderer.h"

#include <chrono>

#include "framework/PassParams.h"
#include "framework/ShaderStandard.h"

#include "core/anim/BoneSkinning.h"
#include "core/anim/VpdLoader.h"
#include "framework/RenderContext.h"
#include "framework/gpu/IGpuDevice.h"
#include "framework/gpu/Types.h"
#include "core/util/Log.h"

#include <algorithm>
#include <filesystem>
#include <stb_image.h>

namespace fs = std::filesystem;

// --- Helpers ---

static std::unique_ptr<Gpu::IGpuTexture> createBoneTexture(const BoneTextureData& data) {
    auto tex = Gpu::device()->createTexture(data.width, data.height, Gpu::TextureFormat::RGBA32F,
                                             data.pixels.data());
    tex->setFilter(Gpu::TextureFilter::Nearest, Gpu::TextureFilter::Nearest);
    tex->setWrap(Gpu::TextureWrap::Clamp, Gpu::TextureWrap::Clamp);
    return tex;
}

// --- ModelRenderer ---

ModelRenderer::ModelRenderer() {
    // 1x1 white dummy texture
    uint8_t white[] = {255, 255, 255, 255};
    mDummyTexture = Gpu::device()->createTexture(1, 1, Gpu::TextureFormat::RGBA8, white);
    mDummyTexture->setFilter(Gpu::TextureFilter::Nearest, Gpu::TextureFilter::Nearest);
    mDummyTexture->setWrap(Gpu::TextureWrap::Clamp, Gpu::TextureWrap::Clamp);
}

ModelRenderer::~ModelRenderer() = default;

void ModelRenderer::loadModel(const PmxModel& model, const fs::path& textureDir) {
    mModel = &model;

    // Compute bounds
    Vec3 minPos = {1e9f, 1e9f, 1e9f}, maxPos = {-1e9f, -1e9f, -1e9f};
    for (const auto& v : model.vertices) {
        minPos.x = std::min(minPos.x, v.position.x);
        minPos.y = std::min(minPos.y, v.position.y);
        minPos.z = std::min(minPos.z, v.position.z);
        maxPos.x = std::max(maxPos.x, v.position.x);
        maxPos.y = std::max(maxPos.y, v.position.y);
        maxPos.z = std::max(maxPos.z, v.position.z);
    }

    mCenter = {
        (minPos.x + maxPos.x) / 2,
        (minPos.y + maxPos.y) / 2,
        (minPos.z + maxPos.z) / 2,
    };
    mMinY = minPos.y;
    mPmxMin = minPos;
    mPmxMax = maxPos;
    Vec3 size = {maxPos.x - minPos.x, maxPos.y - minPos.y, maxPos.z - minPos.z};
    float maxSize = std::max({size.x, size.y, size.z});
    mScale = maxSize > 0 ? 2.0f / maxSize : 1.0f;

    // Model matrix: PMX space → OpenGL display space.
    float s = mScale;
    mModelMat = {s, 0, 0, 0, 0, s, 0, 0, 0, 0, -s, 0, -mCenter.x * s, -mMinY * s, mCenter.z * s, 1};

    MMD_INFO("RENDER", "Model bounds: min=(%.4f,%.4f,%.4f) max=(%.4f,%.4f,%.4f)", minPos.x,
             minPos.y, minPos.z, maxPos.x, maxPos.y, maxPos.z);
    MMD_INFO("RENDER", "Center: (%.4f,%.4f,%.4f) scale: %.4f", mCenter.x, mCenter.y, mCenter.z,
             mScale);

    // Copy indices (shared by all VAOs)
    mIndices.assign(model.indices.begin(), model.indices.end());

    auto tTex = std::chrono::steady_clock::now();
    loadTextures(textureDir);
    MMD_INFO("RENDER", "  Textures: %.0f ms",
             std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - tTex).count());
    buildMaterialBatches(model);
}

void ModelRenderer::loadTextures(const fs::path& textureDir) {
    MMD_INFO("RENDER", "Loading %d textures...", mModel->textureCount());

    for (size_t i = 0; i < mModel->textures.size(); ++i) {
        const auto& texName = mModel->textures[i];
        if (texName.empty()) {
            mTextures.push_back(nullptr);
            continue;
        }

        fs::path texRel = fs::u8path(texName);
        fs::path texPath = textureDir / texRel;
        if (!fs::exists(texPath))
            texPath = textureDir / texRel.filename();

        std::string stripped = texName;
        for (auto& p : {"texture/", "textures/", "tex/", "texture\\", "textures\\", "tex\\"}) {
            std::string lower = texName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.compare(0, strlen(p), p) == 0) {
                stripped = texName.substr(strlen(p));
                break;
            }
        }
        if (!fs::exists(texPath))
            texPath = textureDir / fs::u8path(stripped);

        std::string pathStr = texPath.string();
        int w, h, comp;
        uint8_t* data = stbi_load(pathStr.c_str(), &w, &h, &comp, 4);
        if (!data) {
            texPath = textureDir / fs::u8path(stripped).filename();
            pathStr = texPath.string();
            data = stbi_load(pathStr.c_str(), &w, &h, &comp, 4);
        }

        if (data) {
            auto tex = Gpu::device()->createTexture(w, h, Gpu::TextureFormat::RGBA8, data);
            tex->setFilter(Gpu::TextureFilter::LinearMipmapLinear, Gpu::TextureFilter::Linear);
            tex->setWrap(Gpu::TextureWrap::Repeat, Gpu::TextureWrap::Repeat);
            mTextures.push_back(std::move(tex));
            stbi_image_free(data);
            auto u8name = texPath.filename().u8string();
            std::string name(u8name.begin(), u8name.end());
            MMD_DEBUG("RENDER", "  [%zu] OK: %s", i, name.c_str());
        } else {
            MMD_WARN("RENDER", "  [%zu] Not found: %s", i, texName.c_str());
            mTextures.push_back(nullptr);
        }
    }
}

void ModelRenderer::buildMaterialBatches(const PmxModel& model) {
    mMaterialBatches.reserve(model.materialCount());
    mMaterialColor.reserve(model.materialCount());
    mMaterialEdge.reserve(model.materialCount());
    mMaterialSpecular.reserve(model.materialCount());
    mMaterialAmbient.reserve(model.materialCount());
    mMaterialSphere.reserve(model.materialCount());
    mMaterialToon.reserve(model.materialCount());

    int indexOffset = 0;
    for (int i = 0; i < model.materialCount(); ++i) {
        const auto& mat = model.materials[i];

        MaterialBatch batch;
        batch.first = indexOffset;
        batch.count = mat.vertex_count;
        batch.textureIndex = mat.texture_index;
        batch.materialIndex = i;
        batch.hasEdge = mat.hasFlag(MATERIALFLAG_DRAW_EDGE);
        mMaterialBatches.push_back(batch);

        mMaterialColor.push_back({mat.diffuse_color.x, mat.diffuse_color.y, mat.diffuse_color.z});
        mMaterialEdge.push_back(
            {{mat.edge_color.x, mat.edge_color.y, mat.edge_color.z, mat.edge_color.w},
             mat.edge_size});
        mMaterialSpecular.push_back(
            {{mat.specular_color.x, mat.specular_color.y, mat.specular_color.z},
             mat.specular_factor});
        mMaterialAmbient.push_back({mat.ambient_color.x, mat.ambient_color.y, mat.ambient_color.z});

        MaterialSphere sphere;
        sphere.textureIndex = mat.sphere_texture_index;
        sphere.mode = mat.sphere_mode;
        mMaterialSphere.push_back(sphere);

        MaterialToon toon;
        if (mat.toon_texture_index >= 0) {
            toon.textureIndex = mat.toon_texture_index;
            toon.sharingFlag = mat.toon_sharing_flag;
        } else {
            toon.textureIndex = -1;
        }
        mMaterialToon.push_back(toon);

        indexOffset += mat.vertex_count;
    }
}

// --- Skinning ---

void ModelRenderer::setupSkinning(const PmxModel& model, const fs::path& vpdPath) {
    auto* dev = Gpu::device();

    // Compute skinning matrices and pack to bone texture
    std::vector<float> skinMatrices;
    if (!vpdPath.empty()) {
        auto vpdPoses = VpdLoader::load(vpdPath);
        skinMatrices = BoneSkinning::computeSkinningMatrices(model, vpdPoses);
    } else {
        skinMatrices = BoneSkinning::computeSkinningMatrices(model);
    }
    auto boneData = BoneSkinning::packBoneMatrices(skinMatrices, model.boneCount());
    mBoneTexture = createBoneTexture(boneData);
    mBoneTextureWidth = boneData.width;

    MMD_INFO("RENDER", "Bone texture: %dx%d (%d bones)", boneData.width, boneData.height,
             model.boneCount());

    // Extract skinning vertex data
    auto skinData = BoneSkinning::extractSkinningData(model);

    // Create vertex buffers via device
    int vc3 = (int)skinData.positions.size();
    int vc2 = (int)skinData.uvs.size();

    // Owned by mBaseBuffers to stay alive while VAO references them
    auto& vb = mBaseBuffers;
    vb.clear();
    vb.push_back(dev->createVertexBuffer(skinData.positions.data(),
                                          skinData.positions.size() * sizeof(float),
                                          Gpu::BufferUsage::Static));
    vb.push_back(dev->createVertexBuffer(skinData.normals.data(),
                                          skinData.normals.size() * sizeof(float),
                                          Gpu::BufferUsage::Static));
    vb.push_back(dev->createVertexBuffer(skinData.uvs.data(),
                                          skinData.uvs.size() * sizeof(float),
                                          Gpu::BufferUsage::Static));
    vb.push_back(dev->createVertexBuffer(skinData.boneIndices.data(),
                                          skinData.boneIndices.size() * sizeof(int32_t),
                                          Gpu::BufferUsage::Static));
    vb.push_back(dev->createVertexBuffer(skinData.boneWeights.data(),
                                          skinData.boneWeights.size() * sizeof(float),
                                          Gpu::BufferUsage::Static));
    vb.push_back(dev->createVertexBuffer(skinData.edgeFactors.data(),
                                          skinData.edgeFactors.size() * sizeof(float),
                                          Gpu::BufferUsage::Static));
    auto* posBuf    = vb[0].get();
    auto* normBuf   = vb[1].get();
    auto* uvBuf     = vb[2].get();
    auto* boneIdxBuf = vb[3].get();
    auto* boneWtBuf  = vb[4].get();
    auto* edgeBuf   = vb[5].get();

    // Create zero-initialized morph offset buffers (dynamic)
    std::vector<float> zeroPos(vc3, 0);
    std::vector<float> zeroUv(vc2, 0);
    mMorphVboBuffer = dev->createVertexBuffer(zeroPos.data(), zeroPos.size() * sizeof(float),
                                               Gpu::BufferUsage::Dynamic);
    mUvMorphVboBuffer = dev->createVertexBuffer(zeroUv.data(), zeroUv.size() * sizeof(float),
                                                 Gpu::BufferUsage::Dynamic);

    // Create index buffer
    mIndexBuffer = dev->createIndexBuffer(mIndices.data(), mIndices.size() * sizeof(int32_t),
                                           Gpu::IndexType::UInt32);
    mIndexCount = (int)mIndices.size();

    // Build morph VAO with all attributes (including morph offsets).
    std::vector<Gpu::VertexAttribute> morphAttrs = {
        {ATTR_POSITION,        3, Gpu::DataType::Float,  0, 0},
        {ATTR_NORMAL,          3, Gpu::DataType::Float,  0, 0},
        {ATTR_TEXCOORD,        2, Gpu::DataType::Float,  0, 0},
        {ATTR_BONE_INDICES,    4, Gpu::DataType::Int32,  0, 0},
        {ATTR_BONE_WEIGHTS,    4, Gpu::DataType::Float,  0, 0},
        {ATTR_MORPH_OFFSET,    3, Gpu::DataType::Float,  0, 0},
        {ATTR_UV_MORPH_OFFSET, 2, Gpu::DataType::Float,  0, 0},
        {ATTR_EDGE,            1, Gpu::DataType::Float,  0, 0},
    };

    std::vector<Gpu::IGpuBuffer*> morphBufs = {
        posBuf, normBuf, uvBuf, boneIdxBuf,
        boneWtBuf, mMorphVboBuffer.get(), mUvMorphVboBuffer.get(), edgeBuf,
    };

    mMorphVao = dev->createVertexArray(morphAttrs, morphBufs, mIndexBuffer.get(),
                                        Gpu::IndexType::UInt32, 0, mIndexCount);
}

// --- Rendering ---

void ModelRenderer::renderDepthPass(Gpu::IGpuShader& shader,
                                     const std::array<float, 16>& lightViewProj,
                                     const float* modelMatParam) {
    if (!showModel || mMaterialBatches.empty()) return;

    const float* mm = modelMatParam ? modelMatParam : mModelMat.data();
    shader.use();
    shader.setMat4("u_projMat", lightViewProj.data());
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    shader.setMat4("u_viewMat", identity);
    shader.setMat4("u_modelMat", mm);
    shader.setInt(U_BONE_TEX, TEX_UNIT_BONE);
    shader.setInt(U_BONE_TEX_WIDTH, mBoneTextureWidth);
    shader.setFloat("u_morphWeight", 1.0f);
    mBoneTexture->bind(TEX_UNIT_BONE);

    for (const auto& batch : mMaterialBatches) {
        float alpha = mModel->materials[batch.materialIndex].alpha;
        if (auto* ov = getMaterialOverride(batch.materialIndex)) {
            alpha = ov->alpha;
        }
        shader.setFloat("u_alpha", alpha);
        mMorphVao->draw(Gpu::PrimitiveType::Triangles, batch.count, batch.first);
    }
}

void ModelRenderer::uploadBoneData(const void* data, size_t bytes) {
    if (mBoneTexture)
        mBoneTexture->write(data);
}

void ModelRenderer::renderMorphMainPass(Gpu::IGpuShader& shader,
                                        const std::array<float, 16>& proj,
                                        const std::array<float, 16>& view,
                                        const float* modelMatParam) {
    if (!showModel || mMaterialBatches.empty())
        return;

    const float* modelMatDefault = mModelMat.data();
    const float* mm = modelMatParam ? modelMatParam : modelMatDefault;
    shader.use();
    shader.setMat4(U_PROJ_MAT, proj.data());
    shader.setMat4(U_VIEW_MAT, view.data());
    shader.setMat4(U_MODEL_MAT, mm);
    shader.setInt("u_tex", 0);
    shader.setInt("u_sphereTex", 3);
    shader.setInt("u_toonTex", 4);
    shader.setInt(U_BONE_TEX, TEX_UNIT_BONE);
    shader.setInt(U_BONE_TEX_WIDTH, mBoneTextureWidth);
    shader.setFloat(U_MORPH_WEIGHT, 1.0f);
    mBoneTexture->bind(TEX_UNIT_BONE);

    for (const auto& batch : mMaterialBatches) {
        bool hasTex = (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size() &&
                       mTextures[batch.textureIndex]);
        if (hasTex)
            mTextures[batch.textureIndex]->bind(0);
        else
            mDummyTexture->bind(0);
        const auto& mat = mModel->materials[batch.materialIndex];
        auto* ov = getMaterialOverride(batch.materialIndex);
        shader.setInt(U_HAS_TEXTURE, hasTex ? 1 : 0);
        if (!hasTex) {
            float dx = mMaterialColor[batch.materialIndex].x,
                  dy = mMaterialColor[batch.materialIndex].y,
                  dz = mMaterialColor[batch.materialIndex].z;
            if (ov) {
                dx *= ov->diffuse.x;
                dy *= ov->diffuse.y;
                dz *= ov->diffuse.z;
            }
            shader.setVec3(U_MATERIAL_DIFFUSE, dx, dy, dz);
        }
        shader.setFloat(U_MATERIAL_ALPHA, ov ? ov->alpha : mat.alpha);
        const auto& spec = mMaterialSpecular[batch.materialIndex];
        shader.setVec3(U_SPECULAR_COLOR, ov ? spec.color.x * ov->specular.x : spec.color.x,
                       ov ? spec.color.y * ov->specular.y : spec.color.y,
                       ov ? spec.color.z * ov->specular.z : spec.color.z);
        shader.setFloat(U_SPECULAR_FACTOR, ov ? spec.factor * ov->specularFactor : spec.factor);
        const auto& amb = mMaterialAmbient[batch.materialIndex];
        shader.setVec3(U_MATERIAL_AMBIENT, ov ? amb.x * ov->ambient.x : amb.x,
                       ov ? amb.y * ov->ambient.y : amb.y, ov ? amb.z * ov->ambient.z : amb.z);
        const auto& sphere = mMaterialSphere[batch.materialIndex];
        if (sphere.textureIndex >= 0 && sphere.textureIndex < (int)mTextures.size() &&
            mTextures[sphere.textureIndex]) {
            mTextures[sphere.textureIndex]->bind(3);
            shader.setInt(U_SPHERE_MODE, sphere.mode);
        } else {
            shader.setInt(U_SPHERE_MODE, 0);
        }
        const auto& toon = mMaterialToon[batch.materialIndex];
        if (toon.sharingFlag != 0) {
            int si = toon.textureIndex;
            if (auto* t = mmd::RenderContext::instance().sharedToon(si)) {
                t->bind(4);
                shader.setInt(U_HAS_TOON, 1);
            } else if (auto* t = mmd::RenderContext::instance().sharedToon(0)) {
                t->bind(4);
                shader.setInt(U_HAS_TOON, 1);
            } else {
                shader.setInt(U_HAS_TOON, 0);
            }
        } else if (toon.textureIndex >= 0 && toon.textureIndex < (int)mTextures.size() &&
                   mTextures[toon.textureIndex]) {
            mTextures[toon.textureIndex]->bind(4);
            shader.setInt(U_HAS_TOON, 1);
        } else if (auto* t = mmd::RenderContext::instance().sharedToon(0)) {
            t->bind(4);
            shader.setInt(U_HAS_TOON, 1);
        } else {
            shader.setInt(U_HAS_TOON, 0);
        }

        mMorphVao->draw(Gpu::PrimitiveType::Triangles, batch.count, batch.first);
    }
}

void ModelRenderer::renderMorphOutlinePass(Gpu::IGpuShader& shader,
                                           const std::array<float, 16>& proj,
                                           const std::array<float, 16>& view,
                                           const float* modelMatParam) {
    if (!showModel || !showOutline || mMaterialBatches.empty())
        return;
    const float* modelMatDefault = mModelMat.data();
    const float* mm = modelMatParam ? modelMatParam : modelMatDefault;
    shader.use();
    shader.setMat4(U_PROJ_MAT, proj.data());
    shader.setMat4(U_VIEW_MAT, view.data());
    shader.setMat4(U_MODEL_MAT, mm);
    shader.setInt("u_tex", 0);
    shader.setInt(U_BONE_TEX, TEX_UNIT_BONE);
    shader.setInt(U_BONE_TEX_WIDTH, mBoneTextureWidth);
    shader.setFloat(U_MORPH_WEIGHT, 1.0f);
    mBoneTexture->bind(TEX_UNIT_BONE);

    Gpu::device()->setCullMode(Gpu::CullMode::Front);

    for (const auto& batch : mMaterialBatches) {
        if (!batch.hasEdge)
            continue;
        bool hasTex = (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size() &&
                       mTextures[batch.textureIndex]);
        if (hasTex)
            mTextures[batch.textureIndex]->bind(0);
        else
            mDummyTexture->bind(0);
        const auto& edge = mMaterialEdge[batch.materialIndex];
        auto* ov = getMaterialOverride(batch.materialIndex);
        if (ov) {
            shader.setVec4(U_OUTLINE_COLOR, edge.color.x + ov->edgeColor.x,
                           edge.color.y + ov->edgeColor.y, edge.color.z + ov->edgeColor.z,
                           edge.color.w + ov->edgeColor.w);
            shader.setFloat(U_OUTLINE_THICKNESS, (edge.size + ov->edgeSize) * 0.001f);
            shader.setFloat(U_MATERIAL_ALPHA, ov->alpha);
        } else {
            shader.setVec4(U_OUTLINE_COLOR, edge.color.x, edge.color.y, edge.color.z, edge.color.w);
            shader.setFloat(U_OUTLINE_THICKNESS, edge.size * 0.001f);
            shader.setFloat(U_MATERIAL_ALPHA, mModel->materials[batch.materialIndex].alpha);
        }
        mMorphVao->draw(Gpu::PrimitiveType::Triangles, batch.count, batch.first);
    }
    Gpu::device()->setCullMode(Gpu::CullMode::None);
}

void ModelRenderer::worldAABB(Vec3& outMin, Vec3& outMax) const {
    float s = mScale;
    float cx = mCenter.x, cy = mMinY, cz = mCenter.z;
    float corners[8][3] = {
        {mPmxMin.x, mPmxMin.y, mPmxMin.z}, {mPmxMin.x, mPmxMin.y, mPmxMax.z},
        {mPmxMin.x, mPmxMax.y, mPmxMin.z}, {mPmxMin.x, mPmxMax.y, mPmxMax.z},
        {mPmxMax.x, mPmxMin.y, mPmxMin.z}, {mPmxMax.x, mPmxMin.y, mPmxMax.z},
        {mPmxMax.x, mPmxMax.y, mPmxMin.z}, {mPmxMax.x, mPmxMax.y, mPmxMax.z},
    };
    outMin = {1e9f, 1e9f, 1e9f};
    outMax = {-1e9f, -1e9f, -1e9f};
    for (int i = 0; i < 8; ++i) {
        float wx = s * (corners[i][0] - cx);
        float wy = s * (corners[i][1] - cy);
        float wz = s * (cz - corners[i][2]);
        outMin.x = std::min(outMin.x, wx);
        outMin.y = std::min(outMin.y, wy);
        outMin.z = std::min(outMin.z, wz);
        outMax.x = std::max(outMax.x, wx);
        outMax.y = std::max(outMax.y, wy);
        outMax.z = std::max(outMax.z, wz);
    }
}
