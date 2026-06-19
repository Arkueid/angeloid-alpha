#version 450

layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_color;

layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_modelMat;
    mat4 u_viewMat;
    mat4 u_projMat;
} _ub;

layout(location=0) out vec3 v_color;

void main() {
    gl_Position = _ub.u_projMat * _ub.u_viewMat * _ub.u_modelMat * vec4(in_position, 1.0);
    v_color = in_color;
}
