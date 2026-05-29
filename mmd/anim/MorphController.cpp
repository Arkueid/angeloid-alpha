#include "anim/MorphController.h"

#include <algorithm>
#include <cmath>
#include <cstring>

MorphController::MorphController() = default;

void MorphController::setModel(const PmxModel& model)
{
    mModel = &model;
    mPosOffsets.assign(model.vertexCount() * 3, 0);
    mUvOffsets.assign(model.vertexCount() * 2, 0);
}

void MorphController::setMorphWeight(const std::string& name, float weight)
{
    mMorphWeights[name] = weight;
    updateMorphOffsets();
}

void MorphController::setMorphWeights(const std::unordered_map<std::string, float>& weights)
{
    mMorphWeights = weights;
    updateMorphOffsets();
}

void MorphController::clearMorphs()
{
    mMorphWeights.clear();
    updateMorphOffsets();
}

std::array<float, 4> MorphController::slerpQuat(const std::array<float, 4>& qa,
                                                  const std::array<float, 4>& qb, float t)
{
    if (t <= 0) return qa;
    if (t >= 1) return qb;
    std::array<float, 4> a = qa, b = qb;
    float dot = a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
    if (dot < 0) { b[0] = -b[0]; b[1] = -b[1]; b[2] = -b[2]; b[3] = -b[3]; dot = -dot; }
    if (dot > 0.9995f) {
        std::array<float, 4> r = {a[0] + t*(b[0]-a[0]), a[1] + t*(b[1]-a[1]),
                                   a[2] + t*(b[2]-a[2]), a[3] + t*(b[3]-a[3])};
        float len = std::sqrt(r[0]*r[0] + r[1]*r[1] + r[2]*r[2] + r[3]*r[3]);
        if (len > 0) { r[0]/=len; r[1]/=len; r[2]/=len; r[3]/=len; }
        return r;
    }
    float theta0 = std::acos(std::max(-1.0f, std::min(1.0f, dot)));
    float theta = theta0 * t;
    float st = std::sin(theta), st0 = std::sin(theta0);
    float s1 = std::cos(theta) - dot * st / st0;
    float s2 = st / st0;
    return {s1*a[0] + s2*b[0], s1*a[1] + s2*b[1], s1*a[2] + s2*b[2], s1*a[3] + s2*b[3]};
}

static void applyMorphRecursive(const PmxModel& model, int morphIndex,
                                 float weight,
                                 std::vector<float>& posOffsets,
                                 std::vector<float>& uvOffsets,
                                 int vertexCount,
                                 std::unordered_map<int, MatMorphOverride>& matOverrides,
                                 std::unordered_map<int, BoneMorphTransform>& boneMorphs)
{
    if (morphIndex < 0 || morphIndex >= model.morphCount()) return;
    const auto& morph = model.morphs[morphIndex];

    if (morph.morph_type == MORPH_TYPE_GROUP) {
        for (const auto& offset : morph.offsets) {
            if (auto* g = std::get_if<GroupMorphOffset>(&offset)) {
                applyMorphRecursive(model, g->morph_index,
                    g->value * weight,
                    posOffsets, uvOffsets, vertexCount, matOverrides, boneMorphs);
            }
        }
    } else if (morph.morph_type == MORPH_TYPE_VERTEX) {
        for (const auto& offset : morph.offsets) {
            if (auto* v = std::get_if<VertexMorphOffset>(&offset)) {
                int i = v->vertex_index;
                if (i >= 0 && i < vertexCount) {
                    posOffsets[i*3+0] += v->position_offset.x * weight;
                    posOffsets[i*3+1] += v->position_offset.y * weight;
                    posOffsets[i*3+2] += v->position_offset.z * weight;
                }
            }
        }
    } else if (morph.morph_type == MORPH_TYPE_UV) {
        for (const auto& offset : morph.offsets) {
            if (auto* u = std::get_if<UVMorphOffset>(&offset)) {
                int i = u->vertex_index;
                if (i >= 0 && i < vertexCount) {
                    uvOffsets[i*2+0] += u->uv_offset.x * weight;
                    uvOffsets[i*2+1] += u->uv_offset.y * weight;
                }
            }
        }
    } else if (morph.morph_type == MORPH_TYPE_UV_EXT1 ||
               morph.morph_type == MORPH_TYPE_UV_EXT2 ||
               morph.morph_type == MORPH_TYPE_UV_EXT3 ||
               morph.morph_type == MORPH_TYPE_UV_EXT4) {
        // Extended UV morphs are skipped: the renderer only uses the base UV
        // channel, so there is nowhere to apply these offsets to.
    } else if (morph.morph_type == MORPH_TYPE_MATERIAL) {
        int matCount = model.materialCount();
        for (const auto& offset : morph.offsets) {
            if (auto* m = std::get_if<MaterialMorphOffset>(&offset)) {
                int idx = m->material_index;
                int start = (idx < 0) ? 0 : idx;
                int end = (idx < 0) ? matCount : idx + 1;
                for (int mi = start; mi < end; ++mi) {
                    auto& ov = matOverrides[mi];
                    // Alpha
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
                }
            }
        }
    } else if (morph.morph_type == MORPH_TYPE_BONE) {
        for (const auto& offset : morph.offsets) {
            if (auto* b = std::get_if<BoneMorphOffset>(&offset)) {
                int idx = b->bone_index;
                if (idx < 0) continue;
                auto& bm = boneMorphs[idx];
                bm.translation[0] += b->position.x * weight;
                bm.translation[1] += b->position.y * weight;
                bm.translation[2] += b->position.z * weight;
                std::array<float, 4> mr = {b->rotation.x, b->rotation.y, b->rotation.z, b->rotation.w};
                bm.rotation = MorphController::slerpQuat(bm.rotation, mr, weight);
            }
        }
    }
}

void MorphController::updateMorphOffsets()
{
    if (!mModel) return;

    int vc = mModel->vertexCount();
    std::memset(mPosOffsets.data(), 0, vc * 3 * sizeof(float));
    std::memset(mUvOffsets.data(), 0, vc * 2 * sizeof(float));
    mMaterialOverrides.clear();
    mBoneMorphs.clear();

    for (const auto& [name, weight] : mMorphWeights) {
        if (weight == 0.0f) continue;
        int morphIdx = -1;
        for (const auto& m : mModel->morphs) {
            if (m.name == name || m.english_name == name) { morphIdx = m.index; break; }
        }
        if (morphIdx < 0) continue;
        applyMorphRecursive(*mModel, morphIdx, weight,
                           mPosOffsets, mUvOffsets, vc, mMaterialOverrides, mBoneMorphs);
    }
}

float MorphController::getMaterialAlpha(int materialIndex, float originalAlpha) const
{
    auto it = mMaterialOverrides.find(materialIndex);
    return it != mMaterialOverrides.end() ? it->second.alpha : originalAlpha;
}

const MatMorphOverride* MorphController::getMaterialOverride(int materialIndex) const
{
    auto it = mMaterialOverrides.find(materialIndex);
    return it != mMaterialOverrides.end() ? &it->second : nullptr;
}
