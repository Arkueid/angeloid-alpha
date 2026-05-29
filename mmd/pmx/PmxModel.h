#pragma once

#include "math/VecMath.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

// --- Bone deform types ---
struct Bdef1 {
    int32_t index0 = 0;
};
struct Bdef2 {
    int32_t index0 = 0, index1 = 0;
    float weight0 = 0;
};
struct Bdef4 {
    int32_t index0 = 0, index1 = 0, index2 = 0, index3 = 0;
    float weight0 = 0, weight1 = 0, weight2 = 0, weight3 = 0;
};
struct Sdef {
    int32_t index0 = 0, index1 = 0;
    float weight0 = 0;
    Vec3 sdef_c, sdef_r0, sdef_r1;
};

using BoneDeform = std::variant<Bdef1, Bdef2, Bdef4, Sdef>;

// --- Vertex ---
struct PmxVertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    BoneDeform deform;
    float edge_factor = 0;
};

// --- Material ---
enum {
    MATERIALFLAG_NO_CULL = 0x01,
    MATERIALFLAG_GROUND_SHADOW = 0x02,
    MATERIALFLAG_SELF_SHADOW_MAP = 0x04,
    MATERIALFLAG_SELF_SHADOW = 0x08,
    MATERIALFLAG_DRAW_EDGE = 0x10,
};

enum {
    SPHERE_MODE_NONE = 0,
    SPHERE_MODE_MUL = 1,
    SPHERE_MODE_ADD = 2,
};

struct PmxMaterial {
    std::string name;
    std::string english_name;
    Vec4 diffuse_color;
    float alpha = 1;
    Vec3 specular_color;
    float specular_factor = 1;
    Vec3 ambient_color;
    uint8_t flag = 0;
    Vec4 edge_color;
    float edge_size = 0;
    int32_t texture_index = -1;
    int32_t sphere_texture_index = -1;
    int32_t sphere_mode = SPHERE_MODE_NONE;
    int32_t toon_sharing_flag = 0;
    int32_t toon_texture_index = -1;
    std::string comment;
    int32_t vertex_count = 0;

    bool hasFlag(uint8_t mask) const
    {
        return (flag & mask) != 0;
    }
};

// --- Bone ---
enum {
    BONEFLAG_TAILPOS_IS_BONE = 0x0001,
    BONEFLAG_CAN_ROTATE = 0x0002,
    BONEFLAG_CAN_TRANSLATE = 0x0004,
    BONEFLAG_IS_VISIBLE = 0x0008,
    BONEFLAG_CAN_MANIPULATE = 0x0010,
    BONEFLAG_IS_IK = 0x0020,
    BONEFLAG_IS_EXTERNAL_ROTATION = 0x0100,
    BONEFLAG_IS_EXTERNAL_TRANSLATION = 0x0200,
    BONEFLAG_HAS_FIXED_AXIS = 0x0400,
    BONEFLAG_HAS_LOCAL_COORDINATE = 0x0800,
    BONEFLAG_IS_AFTER_PHYSICS_DEFORM = 0x1000,
    BONEFLAG_IS_EXTERNAL_PARENT_DEFORM = 0x2000,
};

struct PmxIkLink {
    int32_t bone_index = 0;
    bool limit_angle = false;
    Vec3 limit_min;
    Vec3 limit_max;
};

struct PmxIkData {
    int32_t target_index = 0;
    int32_t loop = 0;
    float limit_radian = 0;
    std::vector<PmxIkLink> links;
};

struct PmxBone {
    std::string name;
    std::string english_name;
    Vec3 position;
    int32_t parent_index = -1;
    int32_t layer = 0;
    uint16_t flag = 0;

    Vec3 tail_position;       // set when !hasFlag(BONEFLAG_TAILPOS_IS_BONE)
    int32_t tail_index = -1;  // set when hasFlag(BONEFLAG_TAILPOS_IS_BONE)

    int32_t effect_index = -1;
    float effect_factor = 0;

    Vec3 fixed_axis;
    Vec3 local_x_vector;
    Vec3 local_z_vector;

    int32_t external_key = -1;

    PmxIkData ik;

    // Index in the bones array (set after loading)
    int index = -1;

    bool hasFlag(uint16_t mask) const
    {
        return (flag & mask) != 0;
    }
};

// --- Morph ---
enum {
    MORPH_TYPE_GROUP = 0,
    MORPH_TYPE_VERTEX = 1,
    MORPH_TYPE_BONE = 2,
    MORPH_TYPE_UV = 3,
    MORPH_TYPE_UV_EXT1 = 4,
    MORPH_TYPE_UV_EXT2 = 5,
    MORPH_TYPE_UV_EXT3 = 6,
    MORPH_TYPE_UV_EXT4 = 7,
    MORPH_TYPE_MATERIAL = 8,
};

struct VertexMorphOffset {
    int32_t vertex_index = 0;
    Vec3 position_offset;
};

struct UVMorphOffset {
    int32_t vertex_index = 0;
    Vec4 uv_offset;
};

struct BoneMorphOffset {
    int32_t bone_index = 0;
    Vec3 position;
    Quat rotation;
};

struct MaterialMorphOffset {
    int32_t material_index = 0;
    int32_t calc_mode = 0;
    Vec4 diffuse;
    Vec3 specular;
    float specular_factor = 0;
    Vec3 ambient;
    Vec4 edge_color;
    float edge_size = 0;
    Vec4 texture_factor;
    Vec4 sphere_texture_factor;
    Vec4 toon_texture_factor;
};

struct GroupMorphOffset {
    int32_t morph_index = 0;
    float value = 0;
};

using MorphOffset = std::variant<GroupMorphOffset, VertexMorphOffset, BoneMorphOffset,
                                 UVMorphOffset, MaterialMorphOffset>;

struct PmxMorph {
    std::string name;
    std::string english_name;
    int32_t panel = 0;
    int32_t morph_type = 0;
    std::vector<MorphOffset> offsets;
    int index = -1;
};

// --- RigidBody ---
enum {
    RIGID_SHAPE_SPHERE = 0,
    RIGID_SHAPE_BOX = 1,
    RIGID_SHAPE_CAPSULE = 2,
};

struct PmxRigidBody {
    std::string name;
    std::string english_name;
    int32_t bone_index = -1;
    int32_t collision_group = 0;
    int32_t no_collision_group = 0;
    int32_t shape_type = 0;
    Vec3 shape_size;
    Vec3 shape_position;
    Vec3 shape_rotation;
    float mass = 0;
    float linear_damping = 0;
    float angular_damping = 0;
    float restitution = 0;
    float friction = 0;
    int32_t mode = 0;
    int index = -1;
};

// --- Joint ---
struct PmxJoint {
    std::string name;
    std::string english_name;
    int32_t joint_type = 0;
    int32_t rigidbody_index_a = -1;
    int32_t rigidbody_index_b = -1;
    Vec3 position;
    Vec3 rotation;
    Vec3 translation_limit_min;
    Vec3 translation_limit_max;
    Vec3 rotation_limit_min;
    Vec3 rotation_limit_max;
    Vec3 spring_constant_translation;
    Vec3 spring_constant_rotation;
    int index = -1;
};

// --- Display Slot ---
struct DisplaySlotReference {
    int32_t display_type = 0;  // 0 = bone, 1 = morph
    int32_t ref_index = 0;
};

struct PmxDisplaySlot {
    std::string name;
    std::string english_name;
    int32_t special_flag = 0;
    std::vector<DisplaySlotReference> references;
};

// --- Full Model ---
struct PmxModel {
    float version = 2.0f;
    std::string name;
    std::string english_name;
    std::string comment;
    std::string english_comment;

    std::vector<PmxVertex> vertices;
    std::vector<int32_t> indices;
    std::vector<std::string> textures;
    std::vector<PmxMaterial> materials;
    std::vector<PmxBone> bones;
    std::vector<PmxMorph> morphs;
    std::vector<PmxDisplaySlot> display_slots;
    std::vector<PmxRigidBody> rigidbodies;
    std::vector<PmxJoint> joints;

    int vertexCount() const
    {
        return (int)vertices.size();
    }
    int faceCount() const
    {
        return (int)indices.size() / 3;
    }
    int textureCount() const
    {
        return (int)textures.size();
    }
    int materialCount() const
    {
        return (int)materials.size();
    }
    int boneCount() const
    {
        return (int)bones.size();
    }
    int morphCount() const
    {
        return (int)morphs.size();
    }
};
