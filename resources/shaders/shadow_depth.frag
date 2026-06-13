#version 330 core

uniform float u_alpha;

void main() {
    if (u_alpha < 0.01) discard;
}
