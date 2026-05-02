#version 330 core

in vec3 in_position;
in vec3 in_normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float outline_thickness;

void main() {
    vec3 pos = in_position + in_normal * outline_thickness;
    gl_Position = projection * view * model * vec4(pos, 1.0);
}
