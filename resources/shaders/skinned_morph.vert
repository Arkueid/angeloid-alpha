#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;
layout (location = 3) in ivec4 in_bone_indices;
layout (location = 4) in vec4 in_bone_weights;
layout (location = 5) in vec3 in_morph_offset;
layout (location = 6) in vec2 in_uv_morph_offset;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform sampler2D bone_texture;
uniform int bone_texture_width;
uniform float morph_weight;

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_position;
out vec3 v_world_pos;

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
    vec3 morphed_pos = in_position + in_morph_offset * morph_weight;
    vec2 morphed_uv = in_uv + in_uv_morph_offset * morph_weight;
    
    mat4 skin_matrix = mat4(0.0);
    
    skin_matrix += in_bone_weights.x * fetch_bone_matrix(in_bone_indices.x);
    skin_matrix += in_bone_weights.y * fetch_bone_matrix(in_bone_indices.y);
    skin_matrix += in_bone_weights.z * fetch_bone_matrix(in_bone_indices.z);
    skin_matrix += in_bone_weights.w * fetch_bone_matrix(in_bone_indices.w);
    
    vec4 skinned_pos = skin_matrix * vec4(morphed_pos, 1.0);
    vec4 skinned_normal = skin_matrix * vec4(in_normal, 0.0);
    
    vec4 world_pos = model * skinned_pos;
    v_normal = normalize(mat3(model) * skinned_normal.xyz);
    v_uv = morphed_uv;
    v_position = morphed_pos;
    v_world_pos = world_pos.xyz;
    
    gl_Position = projection * view * world_pos;
}
