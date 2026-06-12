#version 330 core

in vec3 in_position;
in vec3 in_color;
in int bone_index;
in vec3 in_normal;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform sampler2D body_texture;
uniform int body_texture_width;

out vec3 v_color;
out vec3 v_normal;
out vec3 v_worldPos;

mat4 fetch_matrix(int id) {
    int tpm = 4; // texels per matrix
    int gti = id * tpm;
    int row = gti / body_texture_width;
    int cs = gti % body_texture_width;
    vec4 c0 = texelFetch(body_texture, ivec2(cs, row), 0);
    vec4 c1 = texelFetch(body_texture, ivec2(cs + 1, row), 0);
    vec4 c2 = texelFetch(body_texture, ivec2(cs + 2, row), 0);
    vec4 c3 = texelFetch(body_texture, ivec2(cs + 3, row), 0);
    return mat4(c0, c1, c2, c3);
}

void main() {
    mat4 world_mat = mat4(1.0);
    if (bone_index >= 0) {
        world_mat = fetch_matrix(bone_index);
    }
    vec4 world_pos = model * world_mat * vec4(in_position, 1.0);
    gl_Position = projection * view * world_pos;
    v_worldPos = world_pos.xyz;
    v_normal = normalize(mat3(model) * mat3(world_mat) * in_normal);
    v_color = in_color;
}
