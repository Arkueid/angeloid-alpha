#version 330 core

uniform vec3 outline_color;
out vec4 fragColor;

void main() {
    fragColor = vec4(outline_color, 1.0);
}
