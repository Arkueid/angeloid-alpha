#include "framework/opengl/ModelRenderer.h"

#include "framework/opengl/ShaderStandard.h"

#include "core/anim/BoneSkinning.h"
#include "core/anim/VpdLoader.h"
#include "framework/opengl/BoneTextureUtil.h"
#include "framework/opengl/RenderContext.h"
#include "framework/opengl/gpu/Shader.h"
#include "core/util/Log.h"

#include <glad/glad.h>
#include <algorithm>
#include <filesystem>
#include <stb_image.h>

namespace fs = std::filesystem;

ModelRenderer::ModelRenderer() {
    // 1x1 white dummy texture
    uint8_t white[] = {255, 255, 255, 255};
    mDummyTexture = std::make_unique<Gpu::Texture>(1, 1, 4, white);
    mDummyTexture->setFilter(GL_NEAREST, GL_NEAREST);
    mDummyTexture->setWrap(false, false);
}

ModelRenderer::~ModelRenderer() = default;

void ModelRenderer::loadModel(const PmxModel& model, const std::filesystem::path& textureDir) {
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
    Vec3 size = {maxPos.x - minPos.x, maxPos.y - minPos.y, maxPos.z - minPos.z};
    float maxSize = std::max({size.x, size.y, size.z});
    mScale = maxSize > 0 ? 2.0f / maxSize : 1.0f;

    // Model matrix: PMX space → OpenGL display space.
    // Composes: scale(s) · Z-flip · translate to origin at (center.x, minY, center.z)
    //   Z-flip converts right-handed PMX (+Z toward viewer) to left-handed OpenGL (-Z toward viewer).
    //   Center translation normalizes the model around origin.
    //   Scale normalizes the model to fit within ~[-1, 1] NDС.
    // This matrix is passed to the GPU as a uniform; skinning happens in PMX space.
    float s = mScale;
    mModelMat = {s, 0, 0, 0, 0, s, 0, 0, 0, 0, -s, 0, -mCenter.x * s, -mMinY * s, mCenter.z * s, 1};

    MMD_INFO("RENDER", "Model bounds: min=(%.4f,%.4f,%.4f) max=(%.4f,%.4f,%.4f)", minPos.x,
             minPos.y, minPos.z, maxPos.x, maxPos.y, maxPos.z);
    MMD_INFO("RENDER", "Center: (%.4f,%.4f,%.4f) scale: %.4f", mCenter.x, mCenter.y, mCenter.z,
             mScale);

    // Copy indices (shared by all VAOs)
    mIndices.assign(model.indices.begin(), model.indices.end());

    // Load textures
    loadTextures(textureDir);

    // Build material batches
    buildMaterialBatches(model);
}

void ModelRenderer::loadTextures(const std::filesystem::path& textureDir) {
    MMD_INFO("RENDER", "Loading %d textures...", mModel->textureCount());

    for (size_t i = 0; i < mModel->textures.size(); ++i) {
        const auto& texName = mModel->textures[i];
        if (texName.empty()) {
            mTextures.push_back(nullptr);
            continue;
        }

        // PMX texture paths may include subdirectories (e.g. "tex\foo.tga").
        // Try multiple resolution strategies in order:
        //   1. Full relative path under textureDir
        //   2. Just the filename (ignore subdirs)
        //   3. Strip common path prefixes ("texture/", "tex/", etc.) and retry
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
            auto tex = std::make_unique<Gpu::Texture>(w, h, 4, data);
            tex->setFilter(GL_LINEAR, GL_LINEAR);
            tex->setWrap(true, true);
            mTextures.push_back(std::move(tex));
            stbi_image_free(data);
            auto u8name = texPath.filename().u8string();
            std::string name(u8name.begin(), u8name.end());
            MMD_DEBUG("RENDER", "  [%zu] OK: %s", i, name.c_str());
        }
        else {
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

        // toon_sharing_flag != 0 means the material references a shared toon (toon01-10.bmp),
        // where toon_texture_index is the 0-9 index into the shared toon array.
        // Flag == 0 means a per-model toon texture referenced by texture_index.
        MaterialToon toon;
        if (mat.toon_texture_index >= 0) {
            toon.textureIndex = mat.toon_texture_index;
            toon.sharingFlag = mat.toon_sharing_flag;
        }
        else {
            toon.textureIndex = -1;
        }
        mMaterialToon.push_back(toon);

        indexOffset += mat.vertex_count;
    }

}

// --- Skinning ---

void ModelRenderer::setupSkinning(const PmxModel& model, const std::filesystem::path& vpdPath) {
    // Compute skinning matrices and pack to bone texture
    std::vector<float> skinMatrices;
    if (!vpdPath.empty()) {
        auto vpdPoses = VpdLoader::load(vpdPath);
        int matched = 0;
        for (const auto& bone : model.bones) {
            if (vpdPoses.count(bone.name))
                ++matched;
        }
        MMD_INFO("RENDER", "VPD loaded: %zu poses, %d/%d bones matched", vpdPoses.size(), matched,
                 model.boneCount());
        if (matched == 0 && !vpdPoses.empty()) {
            MMD_WARN("RENDER", "  VPD sample: %s", vpdPoses.begin()->first.c_str());
            MMD_WARN("RENDER", "  PMX bone[0]: %s", model.bones[0].name.c_str());
        }
        skinMatrices = BoneSkinning::computeSkinningMatrices(model, vpdPoses);
    }
    else {
        skinMatrices = BoneSkinning::computeSkinningMatrices(model);
    }
    auto boneData = BoneSkinning::packBoneMatrices(skinMatrices, model.boneCount());
    mBoneTexture = createBoneTexture(boneData);
    mBoneTextureWidth = boneData.width;

    MMD_INFO("RENDER", "Bone texture: %dx%d (%d bones)", boneData.width, boneData.height,
             model.boneCount());

    // Extract skinning vertex data (PMX raw coordinates — modelMat handles display transform on GPU)
    auto skinData = BoneSkinning::extractSkinningData(model);

    // Vertex buffer descriptors
    std::vector<Gpu::VertexBufferDesc> descs = {
        {0, skinData.positions.data(), skinData.positions.size() * sizeof(float), 3, GL_FLOAT},
        {1, skinData.normals.data(), skinData.normals.size() * sizeof(float), 3, GL_FLOAT},
        {2, skinData.uvs.data(), skinData.uvs.size() * sizeof(float), 2, GL_FLOAT},
        {3, skinData.boneIndices.data(), skinData.boneIndices.size() * sizeof(int32_t), 4, GL_INT},
        {4, skinData.boneWeights.data(), skinData.boneWeights.size() * sizeof(float), 4, GL_FLOAT},
        {5, skinData.edgeFactors.data(), skinData.edgeFactors.size() * sizeof(float), 1, GL_FLOAT},
    };

    // --- Morph VAOs (skinned + morph_offset + uv_morph_offset + edge_factor VBOs) ---
    std::vector<Gpu::VertexBufferDesc> morphDescs = {
        {0, skinData.positions.data(), skinData.positions.size() * sizeof(float), 3, GL_FLOAT},
        {1, skinData.normals.data(), skinData.normals.size() * sizeof(float), 3, GL_FLOAT},
        {2, skinData.uvs.data(), skinData.uvs.size() * sizeof(float), 2, GL_FLOAT},
        {3, skinData.boneIndices.data(), skinData.boneIndices.size() * sizeof(int32_t), 4, GL_INT},
        {4, skinData.boneWeights.data(), skinData.boneWeights.size() * sizeof(float), 4, GL_FLOAT},
        {7, skinData.edgeFactors.data(), skinData.edgeFactors.size() * sizeof(float), 1, GL_FLOAT},
    };

    int vc3 = (int)skinData.positions.size();
    int vc2 = (int)skinData.uvs.size();

    auto buildMorphVao = [&](Gpu::Vao& vao, GLuint& morphVboId, GLuint& uvVboId) {
        vao = Gpu::Vao::create(morphDescs, mIndices.data(), mIndices.size() * sizeof(int32_t));
        vao.bind();

        // loc 5: morph position offsets (vec3, dynamic)
        glGenBuffers(1, &morphVboId);
        glBindBuffer(GL_ARRAY_BUFFER, morphVboId);
        std::vector<float> zeroPos(vc3, 0);
        glBufferData(GL_ARRAY_BUFFER, zeroPos.size() * sizeof(float), zeroPos.data(),
                     GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        // loc 6: morph UV offsets (vec2, dynamic)
        glGenBuffers(1, &uvVboId);
        glBindBuffer(GL_ARRAY_BUFFER, uvVboId);
        std::vector<float> zeroUv(vc2, 0);
        glBufferData(GL_ARRAY_BUFFER, zeroUv.size() * sizeof(float), zeroUv.data(),
                     GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        vao.vbos.push_back(morphVboId);
        vao.vbos.push_back(uvVboId);
        Gpu::Vao::unbind();
    };

    GLuint morphId, uvId;
    buildMorphVao(mMorphVao, morphId, uvId);
    buildMorphVao(mMorphVaoNoToon, morphId, uvId);
    buildMorphVao(mMorphOutlineVao, morphId, uvId);

    // VBO wrappers point to the first morph VAO's VBOs (they update shared buffers)
    mMorphVboW = std::make_unique<Gpu::VboWrapper>(mMorphVao.vbos[mMorphVao.vbos.size() - 2]);
    mUvMorphVboW = std::make_unique<Gpu::VboWrapper>(mMorphVao.vbos[mMorphVao.vbos.size() - 1]);


}

void ModelRenderer::renderDepthPass(Gpu::ShaderProgram& shader,
                                     const std::array<float, 16>& lightViewProj,
                                     const float* modelMatParam) {
    if (!showModel || mMaterialBatches.empty()) return;

    const float* mm = modelMatParam ? modelMatParam : mModelMat.data();
    shader.use();
    shader.setMat4("u_projMat", lightViewProj.data());
    // Shadow shader uses lightViewProj directly as proj; view is identity
    float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    shader.setMat4("u_viewMat", identity);
    shader.setMat4("u_modelMat", mm);
    shader.setInt(U_BONE_TEX, TEX_UNIT_BONE);
    shader.setInt(U_BONE_TEX_WIDTH, mBoneTextureWidth);
    shader.setFloat("u_morphWeight", 1.0f);
    mBoneTexture->bind(1);

    mMorphVao.bind();
    for (const auto& batch : mMaterialBatches) {
        float alpha = mModel->materials[batch.materialIndex].alpha;
        if (auto* ov = getMaterialOverride(batch.materialIndex)) {
            alpha = ov->alpha;
        }
        shader.setFloat("u_alpha", alpha);
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
                       (void*)(intptr_t)(batch.first * sizeof(int32_t)));
    }
}

void ModelRenderer::uploadBoneData(const void* data, size_t bytes) {
    if (mBoneTexture)
        mBoneTexture->write(data);
}

void ModelRenderer::renderMorphMainPass(Gpu::ShaderProgram& shader,
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
    mBoneTexture->bind(1);

    Gpu::Vao& vao = showToon ? mMorphVao : mMorphVaoNoToon;
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
        }
        else {
            shader.setInt(U_SPHERE_MODE, 0);
        }
        const auto& toon = mMaterialToon[batch.materialIndex];
        if (toon.sharingFlag != 0) {
            int si = toon.textureIndex;
            if (auto* t = mmd::RenderContext::instance().sharedToon(si)) {
                t->bind(4);
                shader.setInt(U_HAS_TOON, 1);
            }
            else if (auto* t = mmd::RenderContext::instance().sharedToon(0)) {
                t->bind(4);
                shader.setInt(U_HAS_TOON, 1);
            }
            else {
                shader.setInt(U_HAS_TOON, 0);
            }
        }
        else if (toon.textureIndex >= 0 && toon.textureIndex < (int)mTextures.size() &&
                 mTextures[toon.textureIndex]) {
            mTextures[toon.textureIndex]->bind(4);
            shader.setInt(U_HAS_TOON, 1);
        }
        else if (auto* t = mmd::RenderContext::instance().sharedToon(0)) {
            t->bind(4);
            shader.setInt(U_HAS_TOON, 1);
        }
        else {
            shader.setInt(U_HAS_TOON, 0);
        }

        vao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
                       (void*)(intptr_t)(batch.first * sizeof(int32_t)));
    }
}

void ModelRenderer::renderMorphOutlinePass(Gpu::ShaderProgram& shader,
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
    mBoneTexture->bind(1);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
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
        }
        else {
            shader.setVec4(U_OUTLINE_COLOR, edge.color.x, edge.color.y, edge.color.z, edge.color.w);
            shader.setFloat(U_OUTLINE_THICKNESS, edge.size * 0.001f);
            shader.setFloat(U_MATERIAL_ALPHA, mModel->materials[batch.materialIndex].alpha);
        }
        mMorphOutlineVao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
                       (void*)(intptr_t)(batch.first * sizeof(int32_t)));
    }
    glDisable(GL_CULL_FACE);
}
