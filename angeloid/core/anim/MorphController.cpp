#include "core/anim/MorphController.h"

#include <algorithm>
#include <cmath>
#include <cstring>

MorphController::MorphController() = default;

void MorphController::setModel(const PmxModel& model) {
    mModel = &model;
    mPosOffsets.assign(model.vertexCount() * 3, 0);
    mUvOffsets.assign(model.vertexCount() * 2, 0);
    mMorphNameIndex.clear();
    for (int i = 0; i < model.morphCount(); ++i) {
        const auto& m = model.morphs[i];
        mMorphNameIndex[m.name] = i;
        if (!m.english_name.empty())
            mMorphNameIndex[m.english_name] = i;
    }
}

void MorphController::setMorphWeight(const std::string& name, float weight) {
    mMorphWeights[name] = weight;
    updateMorphOffsets();
}

void MorphController::setMorphWeights(const std::unordered_map<std::string, float>& weights) {
    mMorphWeights = weights;
    updateMorphOffsets();
}

void MorphController::clearMorphs() {
    mMorphWeights.clear();
    updateMorphOffsets();
}

// Recursively apply morph offsets, handling group morphs by traversing their children.
// Each morph type accumulates into a different buffer:
//   VERTEX → posOffsets (flat float3 per vertex)
//   UV     → uvOffsets (flat float2 per vertex)
//   MATERIAL → matOverrides (per-material alpha/diffuse/specular/edge)
//   BONE   → boneMorphs (per-bone translation + slerped rotation)
//   GROUP  → recurse into children, multiplying their values by the group's weight
//   UV_EXT* → skipped (renderer only uses base UV channel)
static void applyMorphRecursive(const PmxModel& model, int morphIndex, float weight,
                                std::vector<float>& posOffsets, std::vector<float>& uvOffsets,
                                int vertexCount,
                                std::unordered_map<int, MatMorphOverride>& matOverrides,
                                std::unordered_map<int, BoneMorphTransform>& boneMorphs) {
    if (morphIndex < 0 || morphIndex >= model.morphCount())
        return;
    const auto& morph = model.morphs[morphIndex];

    if (morph.morph_type == MORPH_TYPE_GROUP) {
        for (const auto& offset : morph.offsets) {
            if (auto* g = std::get_if<GroupMorphOffset>(&offset)) {
                applyMorphRecursive(model, g->morph_index, g->value * weight, posOffsets, uvOffsets,
                                    vertexCount, matOverrides, boneMorphs);
            }
        }
    }
    else if (morph.morph_type == MORPH_TYPE_VERTEX) {
        for (const auto& offset : morph.offsets) {
            if (auto* v = std::get_if<VertexMorphOffset>(&offset)) {
                int i = v->vertex_index;
                if (i >= 0 && i < vertexCount) {
                    posOffsets[i * 3 + 0] += v->position_offset.x * weight;
                    posOffsets[i * 3 + 1] += v->position_offset.y * weight;
                    posOffsets[i * 3 + 2] += v->position_offset.z * weight;
                }
            }
        }
    }
    else if (morph.morph_type == MORPH_TYPE_UV) {
        for (const auto& offset : morph.offsets) {
            if (auto* u = std::get_if<UVMorphOffset>(&offset)) {
                int i = u->vertex_index;
                if (i >= 0 && i < vertexCount) {
                    uvOffsets[i * 2 + 0] += u->uv_offset.x * weight;
                    uvOffsets[i * 2 + 1] += u->uv_offset.y * weight;
                }
            }
        }
    }
    else if (morph.morph_type == MORPH_TYPE_UV_EXT1 || morph.morph_type == MORPH_TYPE_UV_EXT2 ||
             morph.morph_type == MORPH_TYPE_UV_EXT3 || morph.morph_type == MORPH_TYPE_UV_EXT4) {
        // Extended UV morphs are skipped: the renderer only uses the base UV
        // channel, so there is nowhere to apply these offsets to.
    }
    else if (morph.morph_type == MORPH_TYPE_MATERIAL) {
        int matCount = model.materialCount();
        for (const auto& offset : morph.offsets) {
            if (auto* m = std::get_if<MaterialMorphOffset>(&offset)) {
                // material_index < 0 means "all materials" (global morph)
                int idx = m->material_index;
                int start = (idx < 0) ? 0 : idx;
                int end = (idx < 0) ? matCount : idx + 1;
                for (int mi = start; mi < end; ++mi) {
                    auto it = matOverrides.find(mi);
                    if (it == matOverrides.end()) {
                        it = matOverrides.emplace(mi, MatMorphOverride{}).first;
                        // Seed from base material alpha so multiply-mode has the original value
                        it->second.alpha = model.materials[mi].alpha;
                    }
                    auto& ov = it->second;
                    // calc_mode 0 = multiply (blend toward target), default = additive
                    float curA = ov.alpha;
                    float na = (m->calc_mode == 0)
                                   ? curA * (m->diffuse.w * weight + (1.0f - weight))
                                   : curA + m->diffuse.w * weight;
                    ov.alpha = std::max(0.0f, std::min(1.0f, na));
                    // Diffuse RGB (multiply mode)
                    ov.diffuse.x *= 1.0f + (m->diffuse.x - 1.0f) * weight;
                    ov.diffuse.y *= 1.0f + (m->diffuse.y - 1.0f) * weight;
                    ov.diffuse.z *= 1.0f + (m->diffuse.z - 1.0f) * weight;
                    // Specular
                    ov.specular.x *= 1.0f + (m->specular.x - 1.0f) * weight;
                    ov.specular.y *= 1.0f + (m->specular.y - 1.0f) * weight;
                    ov.specular.z *= 1.0f + (m->specular.z - 1.0f) * weight;
                    ov.specularFactor *= 1.0f + (m->specular_factor - 1.0f) * weight;
                    // Ambient
                    ov.ambient.x *= 1.0f + (m->ambient.x - 1.0f) * weight;
                    ov.ambient.y *= 1.0f + (m->ambient.y - 1.0f) * weight;
                    ov.ambient.z *= 1.0f + (m->ambient.z - 1.0f) * weight;
                    // Edge (additive)
                    ov.edgeColor.x += m->edge_color.x * weight;
                    ov.edgeColor.y += m->edge_color.y * weight;
                    ov.edgeColor.z += m->edge_color.z * weight;
                    ov.edgeColor.w += m->edge_color.w * weight;
                    ov.edgeSize += m->edge_size * weight;
                }
            }
        }
    }
    else if (morph.morph_type == MORPH_TYPE_BONE) {
        for (const auto& offset : morph.offsets) {
            if (auto* b = std::get_if<BoneMorphOffset>(&offset)) {
                int idx = b->bone_index;
                if (idx < 0)
                    continue;
                auto& bm = boneMorphs[idx];
                // Translation is additive (weighted sum)
                bm.translation[0] += b->position.x * weight;
                bm.translation[1] += b->position.y * weight;
                bm.translation[2] += b->position.z * weight;
                // Rotation uses quaternion slerp to properly interpolate on the sphere.
                // Accumulating multiple bone morphs: slerp from current toward each target.
                std::array<float, 4> mr = {b->rotation.x, b->rotation.y, b->rotation.z,
                                           b->rotation.w};
                bm.rotation = quatSlerp(bm.rotation, mr, weight);
            }
        }
    }
}

void MorphController::updateMorphOffsets() {
    if (!mModel)
        return;

    int vc = mModel->vertexCount();
    std::memset(mPosOffsets.data(), 0, vc * 3 * sizeof(float));
    std::memset(mUvOffsets.data(), 0, vc * 2 * sizeof(float));
    mMaterialOverrides.clear();
    mBoneMorphs.clear();

    bool anyActive = false;
    for (const auto& [name, weight] : mMorphWeights) {
        if (weight == 0.0f)
            continue;
        auto it = mMorphNameIndex.find(name);
        int morphIdx = it != mMorphNameIndex.end() ? it->second : -1;
        if (morphIdx < 0)
            continue;
        applyMorphRecursive(*mModel, morphIdx, weight, mPosOffsets, mUvOffsets, vc,
                            mMaterialOverrides, mBoneMorphs);
        anyActive = true;
    }
    if (anyActive || mLastHadActive)
        mOffsetsDirty = true;
    mLastHadActive = anyActive;
}

float MorphController::getMaterialAlpha(int materialIndex, float originalAlpha) const {
    auto it = mMaterialOverrides.find(materialIndex);
    return it != mMaterialOverrides.end() ? it->second.alpha : originalAlpha;
}

const MatMorphOverride* MorphController::getMaterialOverride(int materialIndex) const {
    auto it = mMaterialOverrides.find(materialIndex);
    return it != mMaterialOverrides.end() ? &it->second : nullptr;
}
