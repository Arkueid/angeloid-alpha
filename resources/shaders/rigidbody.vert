#version 330 core

in vec3 in_position;
in vec3 in_color;
in int bone_index;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform sampler2D bone_texture;
uniform int bone_texture_width;

out vec3 v_color;

mat4 fetch_bone_matrix(int bone_id) {
    int tex_width = bone_texture_width;
    int texels_per_matrix = 4;
    int global_texel_idx = bone_id * texels_per_matrix;
    int row = global_texel_idx / tex_width;
    int col_start = global_texel_idx % tex_width;

    vec4 col0 = texelFetch(bone_texture, ivec2(col_start, row), 0);
    vec4 col1 = texelFetch(bone_texture, ivec2(col_start + 1, row), 0);
    vec4 col2 = texelFetch(bone_texture, ivec2(col_start + 2, row), 0);
    vec4 col3 = texelFetch(bone_texture, ivec2(col_start + 3, row), 0);

    return mat4(col0, col1, col2, col3);
}

void main() {
    mat4 skin_matrix;
    if (bone_index >= 0) {
        skin_matrix = fetch_bone_matrix(bone_index);
    } else {
        skin_matrix = mat4(1.0);
    }
    vec4 world_pos = model * skin_matrix * vec4(in_position, 1.0);
    gl_Position = projection * view * world_pos;
    v_color = in_color;
}