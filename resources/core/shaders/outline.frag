#version 330 core

uniform vec3 outline_color;
uniform sampler2D tex;
uniform float alpha;

in vec2 v_uv;
out vec4 fragColor;

void main() {
    if (alpha < 0.01) {
        discard;
    }
    float tex_alpha = texture(tex, v_uv).a;
    if (tex_alpha < 0.5) {
        discard;
    }
    fragColor = vec4(outline_color, 1.0);
}
