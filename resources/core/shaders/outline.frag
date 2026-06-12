#version 330 core

uniform vec4 u_outlineColor;
uniform sampler2D u_tex;
uniform float u_materialAlpha;

in vec2 v_uv;
out vec4 fragColor;

void main() {
    if (u_materialAlpha < 0.01) {
        discard;
    }
    float tex_alpha = texture(u_tex, v_uv).a;
    if (tex_alpha < 0.5) {
        discard;
    }
    fragColor = u_outlineColor;
}
