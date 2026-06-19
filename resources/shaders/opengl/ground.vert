#version 330 core

in vec3 in_position;

uniform mat4 u_projMat;
uniform mat4 u_viewMat;

out vec3 v_world_pos;

void main() {
    vec4 world = vec4(in_position, 1.0);
    v_world_pos = world.xyz;
    gl_Position = u_projMat * u_viewMat * world;
}
