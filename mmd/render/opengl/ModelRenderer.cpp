#include "render/opengl/ModelRenderer.h"
#include "render/opengl/BoneTextureUtil.h"
#include "anim/BoneSkinning.h"
#include "render/opengl/gpu/Shader.h"
#include "anim/VpdLoader.h"

#include <GL/glew.h>
#include <stb_image.h>

#include <algorithm>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// --- Helpers ---

static void buildInterleavedVao(Gpu::Vao& vao,
                                 const std::vector<float>& verts,
                                 const std::vector<int32_t>& indices)
{
    vao.bind();

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    // loc 0: position (3f)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // loc 1: normal (3f)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // loc 2: uv (2f)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    vao.vbos.push_back(vbo);
    vao.setEbo(indices.data(), indices.size() * sizeof(int32_t));
    Gpu::Vao::unbind();
}

// --- ModelRenderer ---

ModelRenderer::ModelRenderer()
{
    // 1x1 white dummy texture
    uint8_t white[] = {255, 255, 255, 255};
    mDummyTexture = std::make_unique<Gpu::Texture>(1, 1, 4, white);
    mDummyTexture->setFilter(GL_NEAREST, GL_NEAREST);
    mDummyTexture->setWrap(false, false);
}

void ModelRenderer::loadModel(const PmxModel& model,
                               const std::filesystem::path& textureDir,
                               const std::filesystem::path& toonDir)
{
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

    // Build model matrix: PMX space -> display space
    // = Z-flip * scale(mScale) * translate(0, -mMinY, 0) * translate(-mCenter.x, 0, -mCenter.z)
    float s = mScale;
    mModelMat = {s,0,0,0, 0,s,0,0, 0,0,-s,0, -mCenter.x*s, -mMinY*s, mCenter.z*s, 1};

    std::cout << "Model bounds: min=(" << minPos.x << "," << minPos.y << "," << minPos.z
              << ") max=(" << maxPos.x << "," << maxPos.y << "," << maxPos.z << ")\n";
    std::cout << "Center: (" << mCenter.x << "," << mCenter.y << "," << mCenter.z
              << ") scale: " << mScale << std::endl;

    // Build interleaved vertex data (PMX raw coordinates)
    mVertices.clear();
    mVertices.reserve(model.vertexCount() * 8);
    for (const auto& v : model.vertices) {
        mVertices.insert(mVertices.end(), {
            v.position.x, v.position.y, v.position.z,
            v.normal.x, v.normal.y, v.normal.z,
            v.uv.x, v.uv.y
        });
    }

    // Copy indices
    mIndices.assign(model.indices.begin(), model.indices.end());

    // Create VAOs (three identical copies for different shader combos)
    buildInterleavedVao(mModelVao, mVertices, mIndices);
    buildInterleavedVao(mToonVao, mVertices, mIndices);
    buildInterleavedVao(mOutlineVao, mVertices, mIndices);

    // Load textures
    loadTextures(textureDir, toonDir);

    // Build material batches
    buildMaterialBatches(model);
}

void ModelRenderer::loadTextures(const std::filesystem::path& textureDir,
                                  const std::filesystem::path& toonDir)
{
    std::cout << "Loading " << mModel->textureCount() << " textures..." << std::endl;

    for (size_t i = 0; i < mModel->textures.size(); ++i) {
        const auto& texName = mModel->textures[i];
        if (texName.empty()) {
            mTextures.push_back(nullptr);
            continue;
        }

        // texName is UTF-8 from PMX, may contain subdirs (e.g. "tex\foo.tga")
        fs::path texRel = fs::u8path(texName);
        // Try full relative path, then just filename, then strip prefixes
        fs::path texPath = textureDir / texRel;
        if (!fs::exists(texPath))
            texPath = textureDir / texRel.filename();

        std::string stripped = texName;
        for (auto& p : {"texture/", "textures/", "tex/", "texture\\", "textures\\", "tex\\"}) {
            std::string lower = texName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.compare(0, strlen(p), p) == 0) { stripped = texName.substr(strlen(p)); break; }
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
            std::cout << "  [" << i << "] OK: " << fs::path(pathStr).filename().string() << std::endl;
        } else {
            std::cout << "  [" << i << "] Not found: " << texName << std::endl;
            mTextures.push_back(nullptr);
        }
    }

    // Load default toon texture
    if (!toonDir.empty()) {
        fs::path toonPath = fs::path(toonDir) / "toon01.bmp";
        int w, h, comp;
        uint8_t* data = stbi_load(toonPath.string().c_str(), &w, &h, &comp, 4);
        if (data) {
            mDefaultToon = std::make_unique<Gpu::Texture>(w, h, 4, data);
            mDefaultToon->setFilter(GL_LINEAR, GL_LINEAR);
            mDefaultToon->setWrap(true, true);
            stbi_image_free(data);
            std::cout << "  [toon] OK: toon01.bmp" << std::endl;
        } else {
            std::cout << "  [toon] Not found: toon01.bmp" << std::endl;
        }
    }
}

void ModelRenderer::buildMaterialBatches(const PmxModel& model)
{
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
        batch.hasEdge = mat.hasFlag(MATERIALFLAG_SELF_SHADOW); // 0x08 matches Python
        mMaterialBatches.push_back(batch);

        mMaterialColor.push_back({
            mat.diffuse_color.x,
            mat.diffuse_color.y,
            mat.diffuse_color.z
        });
        mMaterialEdge.push_back({
            {mat.edge_color.x, mat.edge_color.y, mat.edge_color.z, mat.edge_color.w},
            mat.edge_size
        });
        mMaterialSpecular.push_back({
            {mat.specular_color.x, mat.specular_color.y, mat.specular_color.z},
            mat.specular_factor
        });
        mMaterialAmbient.push_back({
            mat.ambient_color.x,
            mat.ambient_color.y,
            mat.ambient_color.z
        });

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

void ModelRenderer::renderMainPass(Gpu::ShaderProgram& shader,
                                    const std::array<float, 16>& projection,
                                    const std::array<float, 16>& view,
                                    const float* modelMatParam)
{
    if (!showModel || mMaterialBatches.empty()) return;

    const float* modelMatDefault = mModelMat.data();
    const float* modelMat = modelMatParam ? modelMatParam : modelMatDefault;

    shader.use();
    shader.setMat4("projection", projection.data());
    shader.setMat4("view", view.data());
    shader.setMat4("model", modelMat);
    shader.setVec3("light_dir", 0, 0.5f, 1.0f);
    shader.setInt("tex", 0);
    shader.setInt("sphere_tex", 3);
    shader.setInt("toon_tex", 4);

    Gpu::Vao& vao = showToon ? mToonVao : mModelVao;

    for (const auto& batch : mMaterialBatches) {
        // Bind diffuse texture
        bool hasTex = false;
        if (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size()
            && mTextures[batch.textureIndex]) {
            mTextures[batch.textureIndex]->bind(0);
            hasTex = true;
        } else {
            mDummyTexture->bind(0);
        }
        const auto& mat = mModel->materials[batch.materialIndex];
        auto* ov = getMaterialOverride(batch.materialIndex);

        shader.setInt("has_texture", hasTex ? 1 : 0);
        if (!hasTex) {
            float dx = mMaterialColor[batch.materialIndex].x;
            float dy = mMaterialColor[batch.materialIndex].y;
            float dz = mMaterialColor[batch.materialIndex].z;
            if (ov) { dx *= ov->diffuse.x; dy *= ov->diffuse.y; dz *= ov->diffuse.z; }
            shader.setVec3("material_color", dx, dy, dz);
        }
        shader.setFloat("alpha", ov ? ov->alpha : mat.alpha);

        // Specular
        const auto& spec = mMaterialSpecular[batch.materialIndex];
        shader.setVec3("specular_color", spec.color.x, spec.color.y, spec.color.z);
        shader.setFloat("specular_factor", spec.factor);

        // Ambient
        const auto& amb = mMaterialAmbient[batch.materialIndex];
        shader.setVec3("ambient_color", amb.x, amb.y, amb.z);

        // Sphere
        const auto& sphere = mMaterialSphere[batch.materialIndex];
        if (sphere.textureIndex >= 0 && sphere.textureIndex < (int)mTextures.size()
            && mTextures[sphere.textureIndex]) {
            mTextures[sphere.textureIndex]->bind(3);
            shader.setInt("sphere_mode", sphere.mode);
        } else {
            shader.setInt("sphere_mode", 0);
        }

        // Toon
        const auto& toon = mMaterialToon[batch.materialIndex];
        if (toon.textureIndex >= 0 && toon.textureIndex < (int)mTextures.size()
            && mTextures[toon.textureIndex]) {
            mTextures[toon.textureIndex]->bind(4);
            shader.setInt("has_toon", 1);
        } else if (mDefaultToon) {
            mDefaultToon->bind(4);
            shader.setInt("has_toon", 1);
        } else {
            shader.setInt("has_toon", 0);
        }

        vao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
                       (void*)(intptr_t)(batch.first * sizeof(int32_t)));
        Gpu::Vao::unbind();
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}

void ModelRenderer::renderOutlinePass(Gpu::ShaderProgram& shader,
                                       const std::array<float, 16>& projection,
                                       const std::array<float, 16>& view,
                                       const float* modelMatParam)
{
    if (!showModel || !showOutline || mMaterialBatches.empty()) return;

    const float* modelMatDefault = mModelMat.data();
    const float* modelMat = modelMatParam ? modelMatParam : modelMatDefault;

    shader.use();
    shader.setMat4("projection", projection.data());
    shader.setMat4("view", view.data());
    shader.setMat4("model", modelMat);
    shader.setInt("tex", 0);

    glEnable(GL_CULL_FACE);
glCullFace(GL_FRONT);

    for (const auto& batch : mMaterialBatches) {
        if (!batch.hasEdge) continue;

        // Bind texture (for alpha discard in outline frag shader)
        if (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size()
            && mTextures[batch.textureIndex]) {
            mTextures[batch.textureIndex]->bind(0);
        } else {
            mDummyTexture->bind(0);
        }

        const auto& edge = mMaterialEdge[batch.materialIndex];
        const auto& mat = mModel->materials[batch.materialIndex];
        shader.setVec4("outline_color", edge.color.x, edge.color.y, edge.color.z, edge.color.w);
        shader.setFloat("outline_thickness", edge.size * 0.001f);
        auto* ov = getMaterialOverride(batch.materialIndex); float alpha = ov ? ov->alpha : mat.alpha; shader.setFloat("alpha", alpha);

        mOutlineVao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
                       (void*)(intptr_t)(batch.first * sizeof(int32_t)));
        Gpu::Vao::unbind();
    }

    glDisable(GL_CULL_FACE);
}

// --- Skinning ---

void ModelRenderer::setupSkinning(const PmxModel& model, const std::filesystem::path& vpdPath)
{
    // Compute skinning matrices and pack to bone texture
    std::vector<float> skinMatrices;
    if (!vpdPath.empty()) {
        auto vpdPoses = VpdLoader::load(vpdPath);
        int matched = 0;
        for (const auto& bone : model.bones) {
            if (vpdPoses.count(bone.name)) ++matched;
        }
        std::cout << "VPD loaded: " << vpdPoses.size() << " poses, "
                  << matched << "/" << model.boneCount() << " bones matched" << std::endl;
        if (matched == 0 && !vpdPoses.empty()) {
            std::cout << "  VPD sample: " << vpdPoses.begin()->first << std::endl;
            std::cout << "  PMX bone[0]: " << model.bones[0].name << std::endl;
        }
        skinMatrices = BoneSkinning::computeSkinningMatrices(model, vpdPoses);
    } else {
        skinMatrices = BoneSkinning::computeSkinningMatrices(model);
    }
    auto boneData = BoneSkinning::packBoneMatrices(skinMatrices, model.boneCount());
    mBoneTexture = createBoneTexture(boneData);
    mBoneTextureWidth = boneData.width;

    std::cout << "Bone texture: " << boneData.width << "x" << boneData.height
              << " (" << model.boneCount() << " bones)" << std::endl;

    // Extract skinning vertex data (PMX raw coordinates — modelMat handles display transform on GPU)
    auto skinData = BoneSkinning::extractSkinningData(model);

    // Vertex buffer descriptors
    std::vector<Gpu::VertexBufferDesc> descs = {
        {0, skinData.positions.data(), skinData.positions.size() * sizeof(float), 3, GL_FLOAT},
        {1, skinData.normals.data(), skinData.normals.size() * sizeof(float), 3, GL_FLOAT},
        {2, skinData.uvs.data(), skinData.uvs.size() * sizeof(float), 2, GL_FLOAT},
        {3, skinData.boneIndices.data(), skinData.boneIndices.size() * sizeof(int32_t), 4, GL_INT},
        {4, skinData.boneWeights.data(), skinData.boneWeights.size() * sizeof(float), 4, GL_FLOAT},
    };

    // Use the same index data as static VAOs
    mSkinnedVao = Gpu::Vao::create(descs, mIndices.data(),
        mIndices.size() * sizeof(int32_t));
    mSkinnedVaoNoToon = Gpu::Vao::create(descs, mIndices.data(),
        mIndices.size() * sizeof(int32_t));
    mSkinnedOutlineVao = Gpu::Vao::create(descs, mIndices.data(),
        mIndices.size() * sizeof(int32_t));

    // --- Morph VAOs (skinned + morph_offset + uv_morph_offset VBOs) ---
    std::vector<Gpu::VertexBufferDesc> morphDescs = {
        {0, skinData.positions.data(), skinData.positions.size() * sizeof(float), 3, GL_FLOAT},
        {1, skinData.normals.data(), skinData.normals.size() * sizeof(float), 3, GL_FLOAT},
        {2, skinData.uvs.data(), skinData.uvs.size() * sizeof(float), 2, GL_FLOAT},
        {3, skinData.boneIndices.data(), skinData.boneIndices.size() * sizeof(int32_t), 4, GL_INT},
        {4, skinData.boneWeights.data(), skinData.boneWeights.size() * sizeof(float), 4, GL_FLOAT},
    };

    int vc3 = (int)skinData.positions.size();
    int vc2 = (int)skinData.uvs.size();

    auto buildMorphVao = [&](Gpu::Vao& vao, GLuint& morphVboId, GLuint& uvVboId) {
        vao = Gpu::Vao::create(morphDescs, mIndices.data(),
            mIndices.size() * sizeof(int32_t));
        vao.bind();

        // loc 5: morph position offsets (vec3, dynamic)
        glGenBuffers(1, &morphVboId);
        glBindBuffer(GL_ARRAY_BUFFER, morphVboId);
        std::vector<float> zeroPos(vc3, 0);
        glBufferData(GL_ARRAY_BUFFER, zeroPos.size() * sizeof(float), zeroPos.data(), GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        // loc 6: morph UV offsets (vec2, dynamic)
        glGenBuffers(1, &uvVboId);
        glBindBuffer(GL_ARRAY_BUFFER, uvVboId);
        std::vector<float> zeroUv(vc2, 0);
        glBufferData(GL_ARRAY_BUFFER, zeroUv.size() * sizeof(float), zeroUv.data(), GL_DYNAMIC_DRAW);
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
    mMorphVboW = std::make_unique<Gpu::VboWrapper>(
        mMorphVao.vbos[mMorphVao.vbos.size() - 2]);
    mUvMorphVboW = std::make_unique<Gpu::VboWrapper>(
        mMorphVao.vbos[mMorphVao.vbos.size() - 1]);

    useSkinning = true;
}

void ModelRenderer::applyPhysics(const PmxModel& model,
                                 const std::vector<std::array<float, 16>>& physicsMats)
{
    if (!mBoneTexture || !useSkinning) return;
    std::vector<float> skinMatrices = BoneSkinning::computeSkinningMatrices(model);
    BoneSkinning::applyPhysics(model, skinMatrices, physicsMats);
    auto data = BoneSkinning::packBoneMatrices(skinMatrices, model.boneCount());
    mBoneTexture->write(data.pixels.data());
}

void ModelRenderer::updateBoneTexture(const PmxModel& model,
                                       const std::vector<std::array<float, 16>>& poseWorld,
                                       const std::unordered_map<int, BoneMorphTransform>* boneMorphs)
{
    if (!mBoneTexture || !useSkinning) return;

    auto skinMatrices = BoneSkinning::computeSkinningMatrices(model, poseWorld);

    if (boneMorphs && !boneMorphs->empty()) {
        for (const auto& [boneIdx, bm] : *boneMorphs) {
            if (boneIdx < 0 || boneIdx >= model.boneCount()) continue;
            float* M = &skinMatrices[boneIdx * 16];
            float qx = bm.rotation[0], qy = bm.rotation[1], qz = bm.rotation[2], qw = bm.rotation[3];
            float x2 = qx+qx, y2 = qy+qy, z2 = qz+qz;
            float xx = qx*x2, xy = qx*y2, xz = qx*z2;
            float yy = qy*y2, yz = qy*z2, zz = qz*z2;
            float wx = qw*x2, wy = qw*y2, wz = qw*z2;
            float R[9] = {
                1.0f-(yy+zz), xy-wz,       xz+wy,
                xy+wz,       1.0f-(xx+zz), yz-wx,
                xz-wy,       yz+wx,       1.0f-(xx+yy)
            };
            float tx = bm.translation[0] * mScale;
            float ty = bm.translation[1] * mScale;
            float tz = bm.translation[2] * mScale;
            float tmp[12];
            for (int col = 0; col < 3; ++col) {
                for (int row = 0; row < 3; ++row) {
                    tmp[col*4 + row] = R[row*3 + 0] * M[col*4 + 0]
                                     + R[row*3 + 1] * M[col*4 + 1]
                                     + R[row*3 + 2] * M[col*4 + 2];
                }
                tmp[col*4 + 3] = 0;
            }
            for (int row = 0; row < 3; ++row) {
                tmp[12 + row] = R[row*3 + 0] * M[12]
                              + R[row*3 + 1] * M[13]
                              + R[row*3 + 2] * M[14]
                              + (row == 0 ? tx : row == 1 ? ty : tz);
            }
            for (int j = 0; j < 12; ++j) M[j] = tmp[j];
        }
    }

    auto boneData = BoneSkinning::packBoneMatrices(skinMatrices, model.boneCount());
    mBoneTexture->write(boneData.pixels.data());
}

void ModelRenderer::updateBoneTexture(const PmxModel& model,
                                       const std::unordered_map<std::string, VpdPose>& vpdPoses,
                                       const std::unordered_map<std::string,
                                           std::pair<std::array<float,3>, std::array<float,4>>>& vmdTransforms,
                                       const std::unordered_map<int, BoneMorphTransform>* boneMorphs)
{
    if (!mBoneTexture || !useSkinning) return;

    std::vector<float> skinMatrices;
    if (!vmdTransforms.empty()) {
        skinMatrices = BoneSkinning::computeSkinningMatrices(model, vpdPoses, vmdTransforms);
    } else {
        skinMatrices = BoneSkinning::computeSkinningMatrices(model, vpdPoses);
    }

    // Apply bone morph transforms on top
    if (boneMorphs && !boneMorphs->empty()) {
        for (const auto& [boneIdx, bm] : *boneMorphs) {
            if (boneIdx < 0 || boneIdx >= model.boneCount()) continue;
            float* M = &skinMatrices[boneIdx * 16];

            // Quat to matrix
            float qx = bm.rotation[0], qy = bm.rotation[1], qz = bm.rotation[2], qw = bm.rotation[3];
            float x2 = qx+qx, y2 = qy+qy, z2 = qz+qz;
            float xx = qx*x2, xy = qx*y2, xz = qx*z2;
            float yy = qy*y2, yz = qy*z2, zz = qz*z2;
            float wx = qw*x2, wy = qw*y2, wz = qw*z2;
            float R[9] = {
                1.0f-(yy+zz), xy-wz,       xz+wy,
                xy+wz,       1.0f-(xx+zz), yz-wx,
                xz-wy,       yz+wx,       1.0f-(xx+yy)
            };
            float tx = bm.translation[0] * mScale;
            float ty = bm.translation[1] * mScale;
            float tz = bm.translation[2] * mScale;

            // morphMat * M (both column-major 4x4)
            float tmp[12];
            for (int col = 0; col < 3; ++col) {
                for (int row = 0; row < 3; ++row) {
                    tmp[col*4 + row] = R[row*3 + 0] * M[col*4 + 0]
                                     + R[row*3 + 1] * M[col*4 + 1]
                                     + R[row*3 + 2] * M[col*4 + 2];
                }
                tmp[col*4 + 3] = 0;
            }
            // Translation: morphMat * t_M + t_morph
            for (int row = 0; row < 3; ++row) {
                tmp[12 + row] = R[row*3 + 0] * M[12]
                              + R[row*3 + 1] * M[13]
                              + R[row*3 + 2] * M[14]
                              + (row == 0 ? tx : row == 1 ? ty : tz);
            }
            for (int j = 0; j < 12; ++j) M[j] = tmp[j];
        }
    }

    auto boneData = BoneSkinning::packBoneMatrices(skinMatrices, model.boneCount());
    mBoneTexture->write(boneData.pixels.data());
}

void ModelRenderer::renderSkinnedMainPass(Gpu::ShaderProgram& shader,
                                           const std::array<float, 16>& projection,
                                           const std::array<float, 16>& view,
                                           const float* modelMatParam)
{
    if (!showModel || !useSkinning || mMaterialBatches.empty()) return;

    const float* modelMatDefault = mModelMat.data();
    const float* modelMat = modelMatParam ? modelMatParam : modelMatDefault;

    shader.use();
    shader.setMat4("projection", projection.data());
    shader.setMat4("view", view.data());
    shader.setMat4("model", modelMat);
    shader.setVec3("light_dir", 0, 0.5f, 1.0f);
    shader.setInt("tex", 0);
    shader.setInt("sphere_tex", 3);
    shader.setInt("toon_tex", 4);
    shader.setInt("bone_texture", 1);
    shader.setInt("bone_texture_width", mBoneTextureWidth);

    mBoneTexture->bind(1);

    Gpu::Vao& vao = showToon ? mSkinnedVao : mSkinnedVaoNoToon;

    for (const auto& batch : mMaterialBatches) {
        bool hasTex = false;
        if (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size()
            && mTextures[batch.textureIndex]) {
            mTextures[batch.textureIndex]->bind(0);
            hasTex = true;
        } else {
            mDummyTexture->bind(0);
        }
        const auto& mat = mModel->materials[batch.materialIndex];
        auto* ov = getMaterialOverride(batch.materialIndex);
        shader.setInt("has_texture", hasTex ? 1 : 0);
        if (!hasTex) {
            float dx = mMaterialColor[batch.materialIndex].x;
            float dy = mMaterialColor[batch.materialIndex].y;
            float dz = mMaterialColor[batch.materialIndex].z;
            if (ov) { dx *= ov->diffuse.x; dy *= ov->diffuse.y; dz *= ov->diffuse.z; }
            shader.setVec3("material_color", dx, dy, dz);
        }
        shader.setFloat("alpha", ov ? ov->alpha : mat.alpha);

        const auto& spec = mMaterialSpecular[batch.materialIndex];
        shader.setVec3("specular_color", spec.color.x, spec.color.y, spec.color.z);
        shader.setFloat("specular_factor", spec.factor);

        const auto& amb = mMaterialAmbient[batch.materialIndex];
        shader.setVec3("ambient_color", amb.x, amb.y, amb.z);

        const auto& sphere = mMaterialSphere[batch.materialIndex];
        if (sphere.textureIndex >= 0 && sphere.textureIndex < (int)mTextures.size()
            && mTextures[sphere.textureIndex]) {
            mTextures[sphere.textureIndex]->bind(3);
            shader.setInt("sphere_mode", sphere.mode);
        } else {
            shader.setInt("sphere_mode", 0);
        }

        const auto& toon = mMaterialToon[batch.materialIndex];
        if (toon.textureIndex >= 0 && toon.textureIndex < (int)mTextures.size()
            && mTextures[toon.textureIndex]) {
            mTextures[toon.textureIndex]->bind(4);
            shader.setInt("has_toon", 1);
        } else if (mDefaultToon) {
            mDefaultToon->bind(4);
            shader.setInt("has_toon", 1);
        } else {
            shader.setInt("has_toon", 0);
        }

        vao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
                       (void*)(intptr_t)(batch.first * sizeof(int32_t)));
        Gpu::Vao::unbind();
    }
}

void ModelRenderer::renderSkinnedOutlinePass(Gpu::ShaderProgram& shader,
                                              const std::array<float, 16>& projection,
                                              const std::array<float, 16>& view,
                                              const float* modelMatParam)
{
    if (!showModel || !showOutline || !useSkinning || mMaterialBatches.empty()) return;

    const float* modelMatDefault = mModelMat.data();
    const float* modelMat = modelMatParam ? modelMatParam : modelMatDefault;

    shader.use();
    shader.setMat4("projection", projection.data());
    shader.setMat4("view", view.data());
    shader.setMat4("model", modelMat);
    shader.setInt("tex", 0);
    shader.setInt("bone_texture", 1);
    shader.setInt("bone_texture_width", mBoneTextureWidth);

    mBoneTexture->bind(1);

    glEnable(GL_CULL_FACE);
glCullFace(GL_FRONT);

    for (const auto& batch : mMaterialBatches) {
        if (!batch.hasEdge) continue;

        if (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size()
            && mTextures[batch.textureIndex]) {
            mTextures[batch.textureIndex]->bind(0);
        } else {
            mDummyTexture->bind(0);
        }

        const auto& edge = mMaterialEdge[batch.materialIndex];
        const auto& mat = mModel->materials[batch.materialIndex];
        shader.setVec4("outline_color", edge.color.x, edge.color.y, edge.color.z, edge.color.w);
        shader.setFloat("outline_thickness", edge.size * 0.001f);
        auto* ov = getMaterialOverride(batch.materialIndex); float alpha = ov ? ov->alpha : mat.alpha; shader.setFloat("alpha", alpha);

        mSkinnedOutlineVao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT,
                       (void*)(intptr_t)(batch.first * sizeof(int32_t)));
        Gpu::Vao::unbind();
    }

    glDisable(GL_CULL_FACE);
}

void ModelRenderer::renderMorphMainPass(Gpu::ShaderProgram& shader,
                                         const std::array<float, 16>& projection,
                                         const std::array<float, 16>& view,
                                         const float* modelMatParam)
{
    if (!showModel || !useSkinning || mMaterialBatches.empty()) return;

    const float* modelMatDefault = mModelMat.data();
    const float* mm = modelMatParam ? modelMatParam : modelMatDefault;
    shader.use();
    shader.setMat4("projection", projection.data());
    shader.setMat4("view", view.data());
    shader.setMat4("model", mm);
    shader.setVec3("light_dir", 0, 0.5f, 1.0f);
    shader.setInt("tex", 0);
    shader.setInt("sphere_tex", 3);
    shader.setInt("toon_tex", 4);
    shader.setInt("bone_texture", 1);
    shader.setInt("bone_texture_width", mBoneTextureWidth);
    shader.setFloat("morph_weight", 1.0f);
    mBoneTexture->bind(1);

    Gpu::Vao& vao = showToon ? mMorphVao : mMorphVaoNoToon;
    for (const auto& batch : mMaterialBatches) {
        bool hasTex = (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size() && mTextures[batch.textureIndex]);
        if (hasTex) mTextures[batch.textureIndex]->bind(0); else mDummyTexture->bind(0);
        const auto& mat = mModel->materials[batch.materialIndex];
        auto* ov = getMaterialOverride(batch.materialIndex);
        shader.setInt("has_texture", hasTex ? 1 : 0);
        if (!hasTex) { float dx=mMaterialColor[batch.materialIndex].x,dy=mMaterialColor[batch.materialIndex].y,dz=mMaterialColor[batch.materialIndex].z; if(ov){dx*=ov->diffuse.x;dy*=ov->diffuse.y;dz*=ov->diffuse.z;} shader.setVec3("material_color",dx,dy,dz); }
        shader.setFloat("alpha", ov ? ov->alpha : mat.alpha);
        const auto& spec = mMaterialSpecular[batch.materialIndex];
        shader.setVec3("specular_color", ov ? spec.color.x * ov->specular.x : spec.color.x, ov ? spec.color.y * ov->specular.y : spec.color.y, ov ? spec.color.z * ov->specular.z : spec.color.z);
        shader.setFloat("specular_factor", ov ? spec.factor * ov->specularFactor : spec.factor);
        const auto& amb = mMaterialAmbient[batch.materialIndex];
        shader.setVec3("ambient_color", ov ? amb.x * ov->ambient.x : amb.x, ov ? amb.y * ov->ambient.y : amb.y, ov ? amb.z * ov->ambient.z : amb.z);
        const auto& sphere = mMaterialSphere[batch.materialIndex];
        if (sphere.textureIndex >= 0 && sphere.textureIndex < (int)mTextures.size() && mTextures[sphere.textureIndex]) {
            mTextures[sphere.textureIndex]->bind(3);
            shader.setInt("sphere_mode", sphere.mode);
        } else { shader.setInt("sphere_mode", 0); }
        const auto& toon = mMaterialToon[batch.materialIndex];
        if (toon.textureIndex >= 0 && toon.textureIndex < (int)mTextures.size() && mTextures[toon.textureIndex]) {
            mTextures[toon.textureIndex]->bind(4);
            shader.setInt("has_toon", 1);
        } else if (mDefaultToon) {
            mDefaultToon->bind(4);
            shader.setInt("has_toon", 1);
        } else { shader.setInt("has_toon", 0); }

        vao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT, (void*)(intptr_t)(batch.first * sizeof(int32_t)));
        Gpu::Vao::unbind();
    }
}

void ModelRenderer::renderMorphOutlinePass(Gpu::ShaderProgram& shader,
                                            const std::array<float, 16>& projection,
                                            const std::array<float, 16>& view,
                                            const float* modelMatParam)
{
    if (!showModel || !showOutline || !useSkinning || mMaterialBatches.empty()) return;
    const float* modelMatDefault = mModelMat.data();
    const float* mm = modelMatParam ? modelMatParam : modelMatDefault;
    shader.use();
    shader.setMat4("projection", projection.data());
    shader.setMat4("view", view.data());
    shader.setMat4("model", mm);
    shader.setInt("tex", 0);
    shader.setInt("bone_texture", 1);
    shader.setInt("bone_texture_width", mBoneTextureWidth);
    shader.setFloat("morph_weight", 1.0f);
    mBoneTexture->bind(1);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    for (const auto& batch : mMaterialBatches) {
        if (!batch.hasEdge) continue;
        bool hasTex = (batch.textureIndex >= 0 && batch.textureIndex < (int)mTextures.size() && mTextures[batch.textureIndex]);
        if (hasTex) mTextures[batch.textureIndex]->bind(0); else mDummyTexture->bind(0);
        const auto& edge = mMaterialEdge[batch.materialIndex];
        shader.setVec4("outline_color", edge.color.x, edge.color.y, edge.color.z, edge.color.w);
        shader.setFloat("outline_thickness", edge.size * 0.001f);
        auto* ov = getMaterialOverride(batch.materialIndex); float a2 = ov ? ov->alpha : mModel->materials[batch.materialIndex].alpha; shader.setFloat("alpha", a2);
        mMorphOutlineVao.bind();
        glDrawElements(GL_TRIANGLES, batch.count, GL_UNSIGNED_INT, (void*)(intptr_t)(batch.first * sizeof(int32_t)));
        Gpu::Vao::unbind();
    }
    glDisable(GL_CULL_FACE);
}
