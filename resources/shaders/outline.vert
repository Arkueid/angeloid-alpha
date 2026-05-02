#version 330 core

in vec3 in_position;
in vec3 in_normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float outline_thickness;

void main() {
    // 变换法线到视图空间（避免非均匀缩放问题）
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 worldNormal = normalize(normalMatrix * in_normal);
    vec3 worldPos = (model * vec4(in_position, 1.0)).xyz;
    
    // 沿法线外扩
    vec3 expandedPos = worldPos + worldNormal * outline_thickness;
    
    gl_Position = projection * view * vec4(expandedPos, 1.0);
}