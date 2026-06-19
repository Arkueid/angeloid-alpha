#version 450

layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_modelMat;
    int u_boneTexWidth;
    float u_morphWeight;
    float u_outlineThickness;
    vec4 u_outlineColor;
    float u_materialAlpha;
} _ub;

layout(set=0, binding=1) uniform sampler2D u_tex;

layout(location=1) in vec2 v_uv;
layout(location=0) out vec4 fragColor;

void main() {
    if (_ub.u_materialAlpha < 0.01) {
        discard;
    }
    float tex_alpha = texture(u_tex, v_uv).a;
    if (tex_alpha < 0.5) {
        discard;
    }
    fragColor = _ub.u_outlineColor;
}
