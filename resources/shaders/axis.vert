#version 330 core

in vec3 in_position;
in vec3 in_color;

uniform mat4 u_modelMat;
uniform mat4 u_viewMat;
uniform mat4 u_projMat;

out vec3 v_color;

void main() {
    gl_Position = u_projMat * u_viewMat * u_modelMat * vec4(in_position, 1.0);
    v_color = in_color;
}
