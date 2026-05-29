#version 330 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in ivec4 in_bone_indices;
layout(location = 4) in vec4 in_bone_weights;

out vec3 v_normal;
out vec2 v_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float outline_thickness;
uniform sampler2D bone_texture;
uniform int bone_texture_width;

mat4 fetch_bone_matrix(int bone_index) {
    int tex_width = bone_texture_width;
    int texels_per_matrix = 4;

    int global_texel_idx = bone_index * texels_per_matrix;
    int row = global_texel_idx / tex_width;
    int col_start = global_texel_idx % tex_width;

    vec4 col0 = texelFetch(bone_texture, ivec2(col_start, row), 0);
    vec4 col1 = texelFetch(bone_texture, ivec2(col_start + 1, row), 0);
    vec4 col2 = texelFetch(bone_texture, ivec2(col_start + 2, row), 0);
    vec4 col3 = texelFetch(bone_texture, ivec2(col_start + 3, row), 0);

    return mat4(col0, col1, col2, col3);
}

void main() {
    mat4 skin_matrix = mat4(0.0);

    skin_matrix += in_bone_weights.x * fetch_bone_matrix(in_bone_indices.x);
    skin_matrix += in_bone_weights.y * fetch_bone_matrix(in_bone_indices.y);
    skin_matrix += in_bone_weights.z * fetch_bone_matrix(in_bone_indices.z);
    skin_matrix += in_bone_weights.w * fetch_bone_matrix(in_bone_indices.w);

    vec4 skinned_pos = skin_matrix * vec4(in_position, 1.0);
    vec4 skinned_normal = skin_matrix * vec4(in_normal, 0.0);

    mat3 normalMatrix = mat3(model);
    vec3 worldNormal = normalize(normalMatrix * skinned_normal.xyz);
    vec3 worldPos = (model * skinned_pos).xyz;

    vec3 expandedPos = worldPos + worldNormal * outline_thickness;

    gl_Position = projection * view * vec4(expandedPos, 1.0);
    v_normal = worldNormal;
    v_uv = in_uv;
}