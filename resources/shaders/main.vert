#version 330 core

in vec3 in_position;
in vec3 in_normal;
in vec2 in_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 v_normal;
out vec2 v_uv;
out vec3 v_position;
out vec3 v_world_pos;

void main() {
    vec4 world_pos = model * vec4(in_position, 1.0);
    gl_Position = projection * view * world_pos;
    mat3 normal_matrix = transpose(inverse(mat3(model)));
    v_normal = normalize(normal_matrix * in_normal);
    v_uv = in_uv;
    v_position = in_position;
    v_world_pos = world_pos.xyz;
}
