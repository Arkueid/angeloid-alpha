#version 450

layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;
layout(location=2) in vec2 in_uv;
layout(location=3) in ivec4 in_bone_indices;
layout(location=4) in vec4 in_bone_weights;
layout(location=5) in vec3 in_morph_offset;
layout(location=6) in vec2 in_uv_morph_offset;
layout(location=7) in float in_edge_factor;

layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_modelMat;
    int u_boneTexWidth;
    float u_morphWeight;
    float u_outlineThickness;
    vec4 u_outlineColor;
    float u_materialAlpha;
} _ub;

layout(set=0, binding=2) uniform sampler2D u_boneTex;

layout(location=0) out vec3 v_normal;
layout(location=1) out vec2 v_uv;

mat4 fetch_bone_matrix(int bone_index) {
    int tex_width = _ub.u_boneTexWidth;
    int texels_per_matrix = 4;

    int global_texel_idx = bone_index * texels_per_matrix;
    int row = global_texel_idx / tex_width;
    int col_start = global_texel_idx % tex_width;

    vec4 col0 = texelFetch(u_boneTex, ivec2(col_start, row), 0);
    vec4 col1 = texelFetch(u_boneTex, ivec2(col_start + 1, row), 0);
    vec4 col2 = texelFetch(u_boneTex, ivec2(col_start + 2, row), 0);
    vec4 col3 = texelFetch(u_boneTex, ivec2(col_start + 3, row), 0);

    return mat4(col0, col1, col2, col3);
}

void main() {
    vec3 morphed_pos = in_position + in_morph_offset * _ub.u_morphWeight;
    vec2 morphed_uv = in_uv + in_uv_morph_offset * _ub.u_morphWeight;

    mat4 skin_matrix = mat4(0.0);
    skin_matrix += in_bone_weights.x * fetch_bone_matrix(in_bone_indices.x);
    skin_matrix += in_bone_weights.y * fetch_bone_matrix(in_bone_indices.y);
    skin_matrix += in_bone_weights.z * fetch_bone_matrix(in_bone_indices.z);
    skin_matrix += in_bone_weights.w * fetch_bone_matrix(in_bone_indices.w);

    vec4 skinned_pos = skin_matrix * vec4(morphed_pos, 1.0);
    vec4 skinned_normal = skin_matrix * vec4(in_normal, 0.0);

    mat3 normal_matrix = mat3(_ub.u_modelMat);
    vec3 world_normal = normalize(normal_matrix * skinned_normal.xyz);
    vec4 world_pos = _ub.u_modelMat * skinned_pos;

    v_normal = world_normal;
    v_uv = morphed_uv;

    vec3 offset = world_normal * _ub.u_outlineThickness * in_edge_factor;
    gl_Position = _ub.u_projMat * _ub.u_viewMat * (world_pos + vec4(offset, 0.0));
}
