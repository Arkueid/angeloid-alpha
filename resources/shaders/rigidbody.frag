#version 330 core

in vec3 v_color;
in vec3 v_normal;
in vec3 v_worldPos;

uniform vec3 light_dir;

out vec4 frag_color;

void main() {
    vec3 N = normalize(v_normal);
    vec3 L = normalize(-light_dir);
    float diff = max(dot(N, L), 0.0);
    float ambient = 0.25;
    float lit = ambient + diff * (1.0 - ambient);
    frag_color = vec4(v_color * lit, 1.0);
}
