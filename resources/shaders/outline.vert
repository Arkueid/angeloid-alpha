#version 330 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

out vec3 v_normal;
out vec2 v_uv;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float outline_thickness;

void main() {
    mat3 normalMatrix = mat3(model);
    vec3 worldNormal = normalize(normalMatrix * in_normal);
    vec3 worldPos = (model * vec4(in_position, 1.0)).xyz;

    vec3 expandedPos = worldPos + worldNormal * outline_thickness;

    gl_Position = projection * view * vec4(expandedPos, 1.0);
    v_normal = worldNormal;
    v_uv = in_uv;
}
