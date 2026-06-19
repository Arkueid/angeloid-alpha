#version 450

layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_modelMat;
    mat4 u_viewMat;
    mat4 u_projMat;
} _ub;

layout(location=0) in vec3 v_color;
layout(location=0) out vec4 fragColor;

void main() {
    fragColor = vec4(v_color, 1.0);
}
