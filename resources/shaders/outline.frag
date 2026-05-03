#version 330 core

uniform vec3 outline_color;
uniform sampler2D tex;

in vec2 v_uv;
out vec4 fragColor;

void main() {
    float alpha = texture(tex, v_uv).a;
    if (alpha < 0.5) {
        discard;
    }
    fragColor = vec4(outline_color, 1.0);
}
