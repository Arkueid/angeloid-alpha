#version 330 core

in vec3 in_position;
in vec3 in_color;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 bone_matrix;

out vec3 v_color;

void main() {
    vec4 world_pos = model * bone_matrix * vec4(in_position, 1.0);
    gl_Position = projection * view * world_pos;
    v_color = in_color;
}