#include "anim/BoneSkinning.h"

#include "anim/VpdLoader.h"

#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <variant>

// --- Helpers ---

// Unpack a BoneDeform variant into fixed-size index/weight arrays suitable for GPU upload.
// PMX supports 4 deform types; all are normalized to [4 indices, 4 weights]:
//   Bdef1: single bone, weight=1     (indices=[i, 0,0,0], weights=[1, 0,0,0])
//   Bdef2: two bones, weight0 + (1-weight0)  (indices=[i0,i1,0,0])
//   Bdef4: four bones with explicit weights
//   Sdef:  two bones like Bdef2 (SDEF center/radius used only in shader)
static void extractDeform(const BoneDeform& deform, int32_t outIndices[4], float outWeights[4]) {
    std::memset(outIndices, 0, 4 * sizeof(int32_t));
    std::memset(outWeights, 0, 4 * sizeof(float));

    std::visit(
        [&](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, Bdef1>) {
                outIndices[0] = d.index0;
                outWeights[0] = 1.0f;
            }
            else if constexpr (std::is_same_v<T, Bdef2>) {
                outIndices[0] = d.index0;
                outIndices[1] = d.index1;
                outWeights[0] = d.weight0;
                outWeights[1] = 1.0f - d.weight0;
            }
            else if constexpr (std::is_same_v<T, Bdef4>) {
                outIndices[0] = d.index0;
                outIndices[1] = d.index1;
                outIndices[2] = d.index2;
                outIndices[3] = d.index3;
                outWeights[0] = d.weight0;
                outWeights[1] = d.weight1;
                outWeights[2] = d.weight2;
                outWeights[3] = d.weight3;
            }
            else if constexpr (std::is_same_v<T, Sdef>) {
                outIndices[0] = d.index0;
                outIndices[1] = d.index1;
                outWeights[0] = d.weight0;
                outWeights[1] = 1.0f - d.weight0;
            }
        },
        deform);
}

// --- Skinning data extraction ---

SkinningVertexData BoneSkinning::extractSkinningData(const PmxModel& model) {
    SkinningVertexData result;
    int n = model.vertexCount();
    result.positions.reserve(n * 3);
    result.normals.reserve(n * 3);
    result.uvs.reserve(n * 2);
    result.boneIndices.reserve(n * 4);
    result.boneWeights.reserve(n * 4);
    result.edgeFactors.reserve(n);

    for (const auto& v : model.vertices) {
        result.positions.insert(result.positions.end(), {v.position.x, v.position.y, v.position.z});
        result.normals.insert(result.normals.end(), {v.normal.x, v.normal.y, v.normal.z});
        result.uvs.insert(result.uvs.end(), {v.uv.x, v.uv.y});

        int32_t idx[4];
        float wt[4];
        extractDeform(v.deform, idx, wt);
        result.boneIndices.insert(result.boneIndices.end(), idx, idx + 4);
        result.boneWeights.insert(result.boneWeights.end(), wt, wt + 4);
        result.edgeFactors.push_back(v.edge_factor);
    }
    return result;
}

// --- Bone world matrices ---

using Mat4 = std::array<float, 16>;

static Mat4 identityMat() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

static Mat4 translateMat(float x, float y, float z) {
    // Column-major translation: col 3 = (x,y,z,1)
    auto m = identityMat();
    m[12] = x;
    m[13] = y;
    m[14] = z;
    return m;
}

// Column-major matrix multiply: result = A * B
static Mat4 mulMat4(const Mat4& A, const Mat4& B) {
    Mat4 r{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0;
            for (int k = 0; k < 4; ++k) {
                sum += A[k * 4 + row] * B[col * 4 + k];
            }
            r[col * 4 + row] = sum;
        }
    }
    return r;
}

// Column-major matrix inverse for affine transforms (rotation R + translation t).
// For an orthonormal rotation matrix, R⁻¹ = Rᵀ, and t' = -Rᵀ·t.
// This is faster than a general 4×4 inverse and sufficient for bone transforms.
static Mat4 inverseMat4(const Mat4& M) {
    // Extract 3x3 rotation (upper-left) and translation (col 3, rows 0-2)
    // R^T is the inverse of rotation (since R is orthonormal)
    // t_new = -R^T * t
    Mat4 r{};
    // Transpose 3x3
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            r[i * 4 + j] = M[j * 4 + i];  // R^T
    r[3 * 4 + 3] = 1.0f;

    // Translation: -R^T * t
    float tx = M[12], ty = M[13], tz = M[14];
    r[12] = -(r[0] * tx + r[4] * ty + r[8] * tz);
    r[13] = -(r[1] * tx + r[5] * ty + r[9] * tz);
    r[14] = -(r[2] * tx + r[6] * ty + r[10] * tz);
    return r;
}

// Compute the bind-pose world matrix for each bone by walking the hierarchy top-down.
// Bind pose = PMX rest pose (no animation applied). Each bone's world matrix is
//   world_i = world_parent · translate(bone_i.position - parent.position)
// PMX bones store absolute positions; local translation is the delta to the parent.
// This assumes bones are ordered so parents appear before children (PMX serialization guarantees this).
std::vector<Mat4> BoneSkinning::computeBindWorldMatrices(const PmxModel& model) {
    int n = model.boneCount();
    std::vector<Mat4> result(n, identityMat());

    for (int i = 0; i < n; ++i) {
        const auto& bone = model.bones[i];
        if (bone.parent_index >= 0) {
            const auto& parent = model.bones[bone.parent_index];
            float lx = bone.position.x - parent.position.x;
            float ly = bone.position.y - parent.position.y;
            float lz = bone.position.z - parent.position.z;
            auto local = translateMat(lx, ly, lz);
            result[i] = mulMat4(result[bone.parent_index], local);
        }
        else {
            result[i] = translateMat(bone.position.x, bone.position.y, bone.position.z);
        }
    }
    return result;
}

std::vector<Mat4> BoneSkinning::computePoseWorldMatrices(const PmxModel& model) {
    // For static bind pose, pose world = bind world
    return BoneSkinning::computeBindWorldMatrices(model);
}

std::vector<float> BoneSkinning::computeSkinningMatrices(const PmxModel& model) {
    int n = model.boneCount();
    auto bindWorld = BoneSkinning::computeBindWorldMatrices(model);
    auto poseWorld = BoneSkinning::computePoseWorldMatrices(model);

    std::vector<float> result(n * 16);
    for (int i = 0; i < n; ++i) {
        auto invBind = inverseMat4(bindWorld[i]);
        auto M = mulMat4(poseWorld[i], invBind);
        std::memcpy(&result[i * 16], M.data(), 16 * sizeof(float));
    }
    return result;
}

std::vector<Mat4> BoneSkinning::computePoseWorldMatrices(
    const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses) {
    int n = model.boneCount();
    std::vector<Mat4> result(n, identityMat());

    for (int i = 0; i < n; ++i) {
        const auto& bone = model.bones[i];
        Mat4 local = identityMat();

        if (bone.parent_index >= 0) {
            const auto& parent = model.bones[bone.parent_index];
            local[12] = bone.position.x - parent.position.x;
            local[13] = bone.position.y - parent.position.y;
            local[14] = bone.position.z - parent.position.z;

            // Apply VPD pose if present
            auto it = vpdPoses.find(bone.name);
            if (it != vpdPoses.end()) {
                float rot[9];
                it->second.toMatrix(rot);
                // Copy 3x3 rotation into column-major local
                local[0] = rot[0];
                local[4] = rot[1];
                local[8] = rot[2];
                local[1] = rot[3];
                local[5] = rot[4];
                local[9] = rot[5];
                local[2] = rot[6];
                local[6] = rot[7];
                local[10] = rot[8];
                local[12] += it->second.tx;
                local[13] += it->second.ty;
                local[14] += it->second.tz;
            }

            result[i] = mulMat4(result[bone.parent_index], local);
        }
        else {
            local[12] = bone.position.x;
            local[13] = bone.position.y;
            local[14] = bone.position.z;

            auto it = vpdPoses.find(bone.name);
            if (it != vpdPoses.end()) {
                float rot[9];
                it->second.toMatrix(rot);
                local[0] = rot[0];
                local[4] = rot[1];
                local[8] = rot[2];
                local[1] = rot[3];
                local[5] = rot[4];
                local[9] = rot[5];
                local[2] = rot[6];
                local[6] = rot[7];
                local[10] = rot[8];
                local[12] += it->second.tx;
                local[13] += it->second.ty;
                local[14] += it->second.tz;
            }

            result[i] = local;
        }
    }
    return result;
}

std::vector<Mat4> BoneSkinning::computePoseWorldMatrices(
    const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses,
    const std::unordered_map<std::string, std::pair<std::array<float, 3>, std::array<float, 4>>>&
        vmdTransforms) {
    int n = model.boneCount();
    std::vector<Mat4> result(n, identityMat());

    for (int i = 0; i < n; ++i) {
        const auto& bone = model.bones[i];
        Mat4 local = identityMat();

        if (bone.parent_index >= 0) {
            const auto& parent = model.bones[bone.parent_index];
            local[12] = bone.position.x - parent.position.x;
            local[13] = bone.position.y - parent.position.y;
            local[14] = bone.position.z - parent.position.z;
        }
        else {
            local[12] = bone.position.x;
            local[13] = bone.position.y;
            local[14] = bone.position.z;
        }

        // Apply VPD pose
        auto vpdIt = vpdPoses.find(bone.name);
        if (vpdIt != vpdPoses.end()) {
            float rot[9];
            vpdIt->second.toMatrix(rot);
            local[0] = rot[0];
            local[4] = rot[1];
            local[8] = rot[2];
            local[1] = rot[3];
            local[5] = rot[4];
            local[9] = rot[5];
            local[2] = rot[6];
            local[6] = rot[7];
            local[10] = rot[8];
            local[12] += vpdIt->second.tx;
            local[13] += vpdIt->second.ty;
            local[14] += vpdIt->second.tz;
        }

        // VMD applies on top of VPD: VMD rotation REPLACES the local rotation matrix
        // (overwriting any VPD rotation), while position is additive.
        // This matches MMD behavior: VMD bone motion overrides the static pose.
        auto vmdIt = vmdTransforms.find(bone.name);
        if (vmdIt != vmdTransforms.end()) {
            const auto& pos = vmdIt->second.first;
            const auto& rot = vmdIt->second.second;
            float qx = rot[0], qy = rot[1], qz = rot[2], qw = rot[3];
            float x2 = qx + qx, y2 = qy + qy, z2 = qz + qz;
            float xx = qx * x2, xy = qx * y2, xz = qx * z2;
            float yy = qy * y2, yz = qy * z2, zz = qz * z2;
            float wx = qw * x2, wy = qw * y2, wz = qw * z2;
            float r[9] = {1.0f - (yy + zz), xy + wz, xz - wy, xy - wz,         1.0f - (xx + zz),
                          yz + wx,          xz + wy, yz - wx, 1.0f - (xx + yy)};
            // VMD rotation REPLACES local rotation
            local[0] = r[0];
            local[4] = r[3];
            local[8] = r[6];
            local[1] = r[1];
            local[5] = r[4];
            local[9] = r[7];
            local[2] = r[2];
            local[6] = r[5];
            local[10] = r[8];
            local[12] += pos[0];
            local[13] += pos[1];
            local[14] += pos[2];
        }

        if (bone.parent_index >= 0)
            result[i] = mulMat4(result[bone.parent_index], local);
        else
            result[i] = local;
    }
    return result;
}

// After physics updates bone matrices in getBoneTransforms(), bones flagged with
// BONEFLAG_IS_AFTER_PHYSICS_DEFORM need their world matrix recomputed from their
// (possibly physics-modified) parent. These bones are children that should inherit
// their parent's physics-driven motion but don't have their own rigid bodies.
void BoneSkinning::recomputeAfterPhysicsBones(
    const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses,
    std::vector<std::array<float, 16>>& poseWorld) {
    for (int i = 0; i < model.boneCount(); ++i) {
        const auto& bone = model.bones[i];
        if (!bone.hasFlag(BONEFLAG_IS_AFTER_PHYSICS_DEFORM))
            continue;
        if (bone.parent_index < 0)
            continue;

        auto local = identityMat();
        const auto& parent = model.bones[bone.parent_index];
        local[12] = bone.position.x - parent.position.x;
        local[13] = bone.position.y - parent.position.y;
        local[14] = bone.position.z - parent.position.z;

        auto it = vpdPoses.find(bone.name);
        if (it != vpdPoses.end()) {
            float rot[9];
            it->second.toMatrix(rot);
            local[0] = rot[0];
            local[4] = rot[1];
            local[8] = rot[2];
            local[1] = rot[3];
            local[5] = rot[4];
            local[9] = rot[5];
            local[2] = rot[6];
            local[6] = rot[7];
            local[10] = rot[8];
            local[12] += it->second.tx;
            local[13] += it->second.ty;
            local[14] += it->second.tz;
        }

        poseWorld[i] = mulMat4(poseWorld[bone.parent_index], local);
    }
}

std::vector<float> BoneSkinning::computeSkinningMatrices(
    const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses) {
    int n = model.boneCount();
    auto bindWorld = BoneSkinning::computeBindWorldMatrices(model);
    auto poseWorld = BoneSkinning::computePoseWorldMatrices(model, vpdPoses);

    std::vector<float> result(n * 16);
    for (int i = 0; i < n; ++i) {
        auto invBind = inverseMat4(bindWorld[i]);
        auto M = mulMat4(poseWorld[i], invBind);
        std::memcpy(&result[i * 16], M.data(), 16 * sizeof(float));
    }
    return result;
}

std::vector<float> BoneSkinning::computeSkinningMatrices(
    const PmxModel& model, const std::vector<std::array<float, 16>>& poseWorld) {
    int n = model.boneCount();
    auto bindWorld = BoneSkinning::computeBindWorldMatrices(model);

    std::vector<float> result(n * 16);
    for (int i = 0; i < n; ++i) {
        auto invBind = inverseMat4(bindWorld[i]);
        auto M = mulMat4(poseWorld[i], invBind);
        std::memcpy(&result[i * 16], M.data(), 16 * sizeof(float));
    }
    return result;
}

std::vector<float> BoneSkinning::computeSkinningMatrices(
    const PmxModel& model, const std::unordered_map<std::string, VpdPose>& vpdPoses,
    const std::unordered_map<std::string, VmdBoneTransform>& vmdTransforms) {
    int n = model.boneCount();
    auto bindWorld = BoneSkinning::computeBindWorldMatrices(model);

    // Compute pose world with VPD + VMD transforms (hierarchy)
    std::vector<Mat4> poseWorld(n, identityMat());

    for (int i = 0; i < n; ++i) {
        const auto& bone = model.bones[i];
        Mat4 local = identityMat();

        // Translation: relative to parent
        if (bone.parent_index >= 0) {
            const auto& parent = model.bones[bone.parent_index];
            local[12] = bone.position.x - parent.position.x;
            local[13] = bone.position.y - parent.position.y;
            local[14] = bone.position.z - parent.position.z;
        }
        else {
            local[12] = bone.position.x;
            local[13] = bone.position.y;
            local[14] = bone.position.z;
        }

        // Apply VPD pose
        auto vpdIt = vpdPoses.find(bone.name);
        if (vpdIt != vpdPoses.end()) {
            float rot[9];
            vpdIt->second.toMatrix(rot);
            local[0] = rot[0];
            local[4] = rot[1];
            local[8] = rot[2];
            local[1] = rot[3];
            local[5] = rot[4];
            local[9] = rot[5];
            local[2] = rot[6];
            local[6] = rot[7];
            local[10] = rot[8];
            local[12] += vpdIt->second.tx;
            local[13] += vpdIt->second.ty;
            local[14] += vpdIt->second.tz;
        }

        // Apply VMD bone transform (rotation + position offset on top)
        auto vmdIt = vmdTransforms.find(bone.name);
        if (vmdIt != vmdTransforms.end()) {
            const auto& pos = vmdIt->second.first;
            const auto& rot = vmdIt->second.second;
            // Quat to matrix
            float qx = rot[0], qy = rot[1], qz = rot[2], qw = rot[3];
            float x2 = qx + qx, y2 = qy + qy, z2 = qz + qz;
            float xx = qx * x2, xy = qx * y2, xz = qx * z2;
            float yy = qy * y2, yz = qy * z2, zz = qz * z2;
            float wx = qw * x2, wy = qw * y2, wz = qw * z2;
            float r[9] = {1.0f - (yy + zz), xy - wz, xz + wy, xy + wz,         1.0f - (xx + zz),
                          yz - wx,          xz - wy, yz + wx, 1.0f - (xx + yy)};

            // VMD rotation REPLACES existing local rotation (Python: local[:3,:3] = rot_mat)
            local[0] = r[0];
            local[4] = r[1];
            local[8] = r[2];
            local[1] = r[3];
            local[5] = r[4];
            local[9] = r[5];
            local[2] = r[6];
            local[6] = r[7];
            local[10] = r[8];

            local[12] += pos[0];
            local[13] += pos[1];
            local[14] += pos[2];
        }

        // Accumulate hierarchy
        if (bone.parent_index >= 0) {
            poseWorld[i] = mulMat4(poseWorld[bone.parent_index], local);
        }
        else {
            poseWorld[i] = local;
        }
    }

    // Compute skinning matrices (pure world * inv(bind), modelMat handles display transform on GPU)
    std::vector<float> result(n * 16);
    for (int i = 0; i < n; ++i) {
        auto invBind = inverseMat4(bindWorld[i]);
        auto M = mulMat4(poseWorld[i], invBind);
        std::memcpy(&result[i * 16], M.data(), 16 * sizeof(float));
    }
    return result;
}

// --- Packing ---

static int nextPow2(int x) {
    if (x <= 0)
        return 1;
    int p = 1;
    while (p < x)
        p <<= 1;
    return p;
}

void BoneSkinning::applyPhysics(const PmxModel& model, std::vector<float>& skinMatrices,
                                const std::vector<std::array<float, 16>>& physicsMats) {
    auto bindWorld = BoneSkinning::computeBindWorldMatrices(model);

    for (size_t i = 0; i < physicsMats.size() && i < (size_t)model.boneCount(); ++i) {
        bool hasPhysics = false;
        for (int j = 0; j < 16; ++j) {
            if (physicsMats[i][j] != 0) {
                hasPhysics = true;
                break;
            }
        }
        if (!hasPhysics)
            continue;

        auto invBind = inverseMat4(bindWorld[i]);
        auto M = mulMat4(physicsMats[i], invBind);
        for (int j = 0; j < 16; ++j)
            skinMatrices[i * 16 + j] = M[j];
    }
}

// Pack bone matrices into a 2D floating-point texture for GPU skinning.
// Layout: each bone occupies 4 consecutive texels (one per matrix column).
// Texels are laid out row-major: texel index = row * texWidth + col.
// The texture is square, sized to the next power-of-two that fits all texels.
// GPU shader samples this with texelFetch(bone_texture, ivec2(col, row), 0).
BoneTextureData BoneSkinning::packBoneMatrices(const std::vector<float>& matrices, int numBones) {
    int texelsPerMat = 4;
    int totalTexels = numBones * texelsPerMat;
    int texWidth = nextPow2((int)std::ceil(std::sqrt((float)totalTexels)));
    int texHeight = texWidth;

    BoneTextureData result;
    result.width = texWidth;
    result.height = texHeight;
    result.pixels.resize(texWidth * texHeight * 4, 0.0f);

    for (int matIdx = 0; matIdx < numBones; ++matIdx) {
        int globalTexel = matIdx * texelsPerMat;
        int row = globalTexel / texWidth;
        int colStart = globalTexel % texWidth;

        if (colStart + 3 < texWidth) {
            for (int c = 0; c < 4; ++c) {
                int idx = (row * texWidth + colStart + c) * 4;
                const float* col = &matrices[matIdx * 16 + c * 4];
                result.pixels[idx + 0] = col[0];
                result.pixels[idx + 1] = col[1];
                result.pixels[idx + 2] = col[2];
                result.pixels[idx + 3] = col[3];
            }
        }
    }
    return result;
}
