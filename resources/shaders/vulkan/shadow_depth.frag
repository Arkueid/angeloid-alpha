#version 450

// Combined block (same as in vertex shader)
layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_modelMat;
    int u_boneTexWidth;
    float u_morphWeight;
    float u_alpha;
} _ub;

void main() {
    if (_ub.u_alpha < 0.01) discard;
}
