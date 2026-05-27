#include "pmx/PmxReader.h"
#include "encoding/Encoding.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

// --- BinaryReader ---

BinaryReader::BinaryReader(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    mSize = file.tellg();
    file.seekg(0);
    mData.resize(mSize);
    file.read(reinterpret_cast<char*>(mData.data()), mSize);
    mPos = 0;
}

void BinaryReader::readBytes(void* dst, size_t len)
{
    std::memcpy(dst, &mData[mPos], len);
    mPos += len;
}

int8_t  BinaryReader::readS8()    { return readVal<int8_t>(); }
uint8_t BinaryReader::readU8()    { return readVal<uint8_t>(); }
int16_t BinaryReader::readS16()   { return readVal<int16_t>(); }
uint16_t BinaryReader::readU16()  { return readVal<uint16_t>(); }
int32_t BinaryReader::readS32()   { return readVal<int32_t>(); }
uint32_t BinaryReader::readU32()  { return readVal<uint32_t>(); }
float   BinaryReader::readF32()   { return readVal<float>(); }

int32_t BinaryReader::readIndex(uint8_t size)
{
    if (size == 1) return readS8();
    if (size == 2) return readS16();
    return readS32();
}

uint32_t BinaryReader::readVertexIndex(uint8_t size)
{
    if (size <= 2) {
        if (size == 1) return readU8();
        return readU16();
    }
    return readS32();
}

Vec2 BinaryReader::readVec2() { return {readF32(), readF32()}; }
Vec3 BinaryReader::readVec3() { return {readF32(), readF32(), readF32()}; }
Vec4 BinaryReader::readVec4() { return {readF32(), readF32(), readF32(), readF32()}; }
Quat BinaryReader::readQuat() { return {readF32(), readF32(), readF32(), readF32()}; }

std::string BinaryReader::readText(uint8_t textEncoding)
{
    int32_t len = readS32();
    if (len <= 0) return {};

    if (textEncoding == 0) {
        std::vector<uint16_t> raw(len / 2);
        readBytes(raw.data(), len);
        return Encoding::utf16leToUtf8(raw.data(), raw.size());
    } else {
        // UTF-8
        std::string result(len, '\0');
        readBytes(result.data(), len);
        return result;
    }
}

// --- Bone deform reader ---
static BoneDeform readDeform(BinaryReader& r, uint8_t boneIndexSize)
{
    uint8_t deformType = r.readU8();
    if (deformType == 0) {
        return Bdef1{r.readIndex(boneIndexSize)};
    } else if (deformType == 1) {
        return Bdef2{
            r.readIndex(boneIndexSize),
            r.readIndex(boneIndexSize),
            r.readF32()
        };
    } else if (deformType == 2) {
        return Bdef4{
            r.readIndex(boneIndexSize),
            r.readIndex(boneIndexSize),
            r.readIndex(boneIndexSize),
            r.readIndex(boneIndexSize),
            r.readF32(), r.readF32(), r.readF32(), r.readF32()
        };
    } else if (deformType == 3) {
        return Sdef{
            r.readIndex(boneIndexSize),
            r.readIndex(boneIndexSize),
            r.readF32(),
            r.readVec3(), r.readVec3(), r.readVec3()
        };
    }
    throw std::runtime_error("Unknown deform type: " + std::to_string(deformType));
}

// --- PMX Parser ---
PmxModel PmxReader::load(const std::filesystem::path& path)
{
    BinaryReader r(path);
    PmxModel model;

    // Header
    char sig[4];
    r.readBytes(sig, 4);
    if (std::memcmp(sig, "PMX ", 4) != 0) {
        throw std::runtime_error("Invalid PMX signature");
    }

    model.version = r.readF32();
    if (model.version != 2.0f && model.version != 2.1f) {
        throw std::runtime_error("Unsupported PMX version: " + std::to_string(model.version));
    }

    // Flags
    uint8_t flagBytes = r.readU8();
    if (flagBytes != 8) {
        throw std::runtime_error("Invalid flag length: " + std::to_string(flagBytes));
    }

    uint8_t textEncoding = r.readU8();
    uint8_t extendedUV = r.readU8();
    uint8_t vertexIndexSize = r.readU8();
    uint8_t textureIndexSize = r.readU8();
    uint8_t materialIndexSize = r.readU8();
    uint8_t boneIndexSize = r.readU8();
    uint8_t morphIndexSize = r.readU8();
    uint8_t rigidbodyIndexSize = r.readU8();

    // Model info
    model.name = r.readText(textEncoding);
    model.english_name = r.readText(textEncoding);
    model.comment = r.readText(textEncoding);
    model.english_comment = r.readText(textEncoding);

    // Vertices
    int32_t vertexCount = r.readS32();
    model.vertices.reserve(vertexCount);
    for (int32_t i = 0; i < vertexCount; ++i) {
        PmxVertex v;
        v.position = r.readVec3();
        v.normal = r.readVec3();
        v.uv = r.readVec2();
        // Skip extended UVs
        for (uint8_t e = 0; e < extendedUV; ++e) {
            r.readVec4();
        }
        v.deform = readDeform(r, boneIndexSize);
        v.edge_factor = r.readF32();
        model.vertices.push_back(std::move(v));
    }

    // Indices
    int32_t indexCount = r.readS32();
    model.indices.reserve(indexCount);
    for (int32_t i = 0; i < indexCount; ++i) {
        model.indices.push_back((int32_t)r.readVertexIndex(vertexIndexSize));
    }

    // Textures
    int32_t textureCount = r.readS32();
    model.textures.reserve(textureCount);
    for (int32_t i = 0; i < textureCount; ++i) {
        model.textures.push_back(r.readText(textEncoding));
    }

    // Materials
    int32_t materialCount = r.readS32();
    model.materials.reserve(materialCount);
    for (int32_t i = 0; i < materialCount; ++i) {
        PmxMaterial mat;
        mat.name = r.readText(textEncoding);
        mat.english_name = r.readText(textEncoding);
        // PMX stores diffuse as RGB (Vec3) then alpha separately
        Vec3 diffuseRgb = r.readVec3();
        mat.alpha = r.readF32();
        mat.diffuse_color = {diffuseRgb.x, diffuseRgb.y, diffuseRgb.z, mat.alpha};
        mat.specular_color = r.readVec3();
        mat.specular_factor = r.readF32();
        mat.ambient_color = r.readVec3();
        mat.flag = r.readU8();
        mat.edge_color = r.readVec4();
        mat.edge_size = r.readF32();
        mat.texture_index = r.readIndex(textureIndexSize);
        mat.sphere_texture_index = r.readIndex(textureIndexSize);
        mat.sphere_mode = r.readS8();

        mat.toon_sharing_flag = r.readS8();
        if (mat.toon_sharing_flag == 0) {
            mat.toon_texture_index = r.readIndex(textureIndexSize);
        } else {
            mat.toon_texture_index = r.readS8();
        }

        mat.comment = r.readText(textEncoding);
        mat.vertex_count = r.readS32();
        model.materials.push_back(std::move(mat));
    }

    // Bones
    int32_t boneCount = r.readS32();
    model.bones.reserve(boneCount);
    for (int32_t i = 0; i < boneCount; ++i) {
        PmxBone bone;
        bone.name = r.readText(textEncoding);
        bone.english_name = r.readText(textEncoding);
        bone.position = r.readVec3();
        bone.parent_index = r.readIndex(boneIndexSize);
        bone.layer = r.readS32();
        bone.flag = r.readU16();

        if (!bone.hasFlag(BONEFLAG_TAILPOS_IS_BONE)) {
            bone.tail_position = r.readVec3();
        } else {
            bone.tail_index = r.readIndex(boneIndexSize);
        }

        if (bone.hasFlag(BONEFLAG_IS_EXTERNAL_ROTATION) || bone.hasFlag(BONEFLAG_IS_EXTERNAL_TRANSLATION)) {
            bone.effect_index = r.readIndex(boneIndexSize);
            bone.effect_factor = r.readF32();
        }

        if (bone.hasFlag(BONEFLAG_HAS_FIXED_AXIS)) {
            bone.fixed_axis = r.readVec3();
        }

        if (bone.hasFlag(BONEFLAG_HAS_LOCAL_COORDINATE)) {
            bone.local_x_vector = r.readVec3();
            bone.local_z_vector = r.readVec3();
        }

        if (bone.hasFlag(BONEFLAG_IS_EXTERNAL_PARENT_DEFORM)) {
            bone.external_key = r.readS32();
        }

        if (bone.hasFlag(BONEFLAG_IS_IK)) {
            bone.ik.target_index = r.readIndex(boneIndexSize);
            bone.ik.loop = r.readS32();
            bone.ik.limit_radian = r.readF32();

            int32_t linkCount = r.readS32();
            bone.ik.links.reserve(linkCount);
            for (int32_t li = 0; li < linkCount; ++li) {
                PmxIkLink link;
                link.bone_index = r.readIndex(boneIndexSize);
                link.limit_angle = r.readU8() != 0;
                if (link.limit_angle) {
                    link.limit_min = r.readVec3();
                    link.limit_max = r.readVec3();
                }
                bone.ik.links.push_back(link);
            }
        }

        bone.index = i;
        model.bones.push_back(std::move(bone));
    }

    // Morphs
    int32_t morphCount = r.readS32();
    model.morphs.reserve(morphCount);
    for (int32_t i = 0; i < morphCount; ++i) {
        PmxMorph morph;
        morph.name = r.readText(textEncoding);
        morph.english_name = r.readText(textEncoding);
        morph.panel = r.readS8();
        morph.morph_type = r.readS8();
        int32_t offsetCount = r.readS32();

        for (int32_t oi = 0; oi < offsetCount; ++oi) {
            if (morph.morph_type == MORPH_TYPE_GROUP) {
                morph.offsets.push_back(GroupMorphOffset{
                    r.readIndex(morphIndexSize),
                    r.readF32()
                });
            } else if (morph.morph_type == MORPH_TYPE_VERTEX) {
                morph.offsets.push_back(VertexMorphOffset{
                    (int32_t)r.readVertexIndex(vertexIndexSize),
                    r.readVec3()
                });
            } else if (morph.morph_type == MORPH_TYPE_BONE) {
                morph.offsets.push_back(BoneMorphOffset{
                    r.readIndex(boneIndexSize),
                    r.readVec3(),
                    r.readQuat()
                });
            } else if (morph.morph_type == MORPH_TYPE_UV ||
                       morph.morph_type == MORPH_TYPE_UV_EXT1 ||
                       morph.morph_type == MORPH_TYPE_UV_EXT2 ||
                       morph.morph_type == MORPH_TYPE_UV_EXT3 ||
                       morph.morph_type == MORPH_TYPE_UV_EXT4) {
                morph.offsets.push_back(UVMorphOffset{
                    (int32_t)r.readVertexIndex(vertexIndexSize),
                    r.readVec4()
                });
            } else if (morph.morph_type == MORPH_TYPE_MATERIAL) {
                MaterialMorphOffset mmo;
                mmo.material_index = r.readIndex(materialIndexSize);
                mmo.calc_mode = r.readS8();
                mmo.diffuse = r.readVec4();
                mmo.specular = r.readVec3();
                mmo.specular_factor = r.readF32();
                mmo.ambient = r.readVec3();
                mmo.edge_color = r.readVec4();
                mmo.edge_size = r.readF32();
                mmo.texture_factor = r.readVec4();
                mmo.sphere_texture_factor = r.readVec4();
                mmo.toon_texture_factor = r.readVec4();
                morph.offsets.push_back(std::move(mmo));
            }
        }

        morph.index = i;
        model.morphs.push_back(std::move(morph));
    }

    // Display slots
    int32_t slotCount = r.readS32();
    model.display_slots.reserve(slotCount);
    for (int32_t i = 0; i < slotCount; ++i) {
        PmxDisplaySlot slot;
        slot.name = r.readText(textEncoding);
        slot.english_name = r.readText(textEncoding);
        slot.special_flag = r.readS8();
        int32_t refCount = r.readS32();
        slot.references.reserve(refCount);
        for (int32_t ri = 0; ri < refCount; ++ri) {
            int32_t displayType = r.readS8();
            if (displayType == 0) {
                slot.references.push_back({displayType, r.readIndex(boneIndexSize)});
            } else {
                slot.references.push_back({displayType, r.readIndex(morphIndexSize)});
            }
        }
        model.display_slots.push_back(std::move(slot));
    }

    // Rigidbodies
    int32_t rbCount = r.readS32();
    model.rigidbodies.reserve(rbCount);
    for (int32_t i = 0; i < rbCount; ++i) {
        PmxRigidBody rb;
        rb.name = r.readText(textEncoding);
        rb.english_name = r.readText(textEncoding);
        rb.bone_index = r.readIndex(boneIndexSize);
        rb.collision_group = r.readS8();
        rb.no_collision_group = r.readU16();
        rb.shape_type = r.readS8();
        rb.shape_size = r.readVec3();
        rb.shape_position = r.readVec3();
        rb.shape_rotation = r.readVec3();
        rb.mass = r.readF32();
        rb.linear_damping = r.readF32();
        rb.angular_damping = r.readF32();
        rb.restitution = r.readF32();
        rb.friction = r.readF32();
        rb.mode = r.readS8();
        rb.index = i;
        model.rigidbodies.push_back(std::move(rb));
    }

    // Joints
    int32_t jointCount = r.readS32();
    model.joints.reserve(jointCount);
    for (int32_t i = 0; i < jointCount; ++i) {
        PmxJoint joint;
        joint.name = r.readText(textEncoding);
        joint.english_name = r.readText(textEncoding);
        joint.joint_type = r.readS8();
        joint.rigidbody_index_a = r.readIndex(rigidbodyIndexSize);
        joint.rigidbody_index_b = r.readIndex(rigidbodyIndexSize);
        joint.position = r.readVec3();
        joint.rotation = r.readVec3();
        joint.translation_limit_min = r.readVec3();
        joint.translation_limit_max = r.readVec3();
        joint.rotation_limit_min = r.readVec3();
        joint.rotation_limit_max = r.readVec3();
        joint.spring_constant_translation = r.readVec3();
        joint.spring_constant_rotation = r.readVec3();
        joint.index = i;
        model.joints.push_back(std::move(joint));
    }

    return model;
}
