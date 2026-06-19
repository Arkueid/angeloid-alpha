#version 450

layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;
layout(location=2) in vec2 in_uv;
layout(location=3) in ivec4 in_bone_indices;
layout(location=4) in vec4 in_bone_weights;
layout(location=5) in vec3 in_morph_offset;
layout(location=6) in vec2 in_uv_morph_offset;

// Combined block: vertex + fragment uniforms
layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_modelMat;
    int u_boneTexWidth;
    float u_morphWeight;
    float u_alpha;
} _ub;

layout(set=0, binding=2) uniform sampler2D u_boneTex;

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
    mat4 skin_matrix = mat4(0.0);
    skin_matrix += in_bone_weights.x * fetch_bone_matrix(in_bone_indices.x);
    skin_matrix += in_bone_weights.y * fetch_bone_matrix(in_bone_indices.y);
    skin_matrix += in_bone_weights.z * fetch_bone_matrix(in_bone_indices.z);
    skin_matrix += in_bone_weights.w * fetch_bone_matrix(in_bone_indices.w);
    vec4 world_pos = _ub.u_modelMat * skin_matrix * vec4(morphed_pos, 1.0);
    gl_Position = _ub.u_projMat * _ub.u_viewMat * world_pos;
}
