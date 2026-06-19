#version 450

layout(location=0) in vec3 in_position;

layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_lightViewProj;
    int u_hasShadow;
} _ub;

layout(location=0) out vec3 v_world_pos;

void main() {
    vec4 world = vec4(in_position, 1.0);
    v_world_pos = world.xyz;
    gl_Position = _ub.u_projMat * _ub.u_viewMat * world;
}
