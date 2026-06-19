#version 330 core

in vec3 in_position;
in vec3 in_color;
in int bone_index;
in vec3 in_normal;

uniform mat4 u_projMat;
uniform mat4 u_viewMat;
uniform mat4 u_modelMat;
uniform sampler2D u_boneTex;
uniform int u_boneTexWidth;

out vec3 v_color;
out vec3 v_normal;
out vec3 v_worldPos;

mat4 fetch_matrix(int id) {
    int tpm = 4;
    int gti = id * tpm;
    int row = gti / u_boneTexWidth;
    int cs = gti % u_boneTexWidth;
    vec4 c0 = texelFetch(u_boneTex, ivec2(cs, row), 0);
    vec4 c1 = texelFetch(u_boneTex, ivec2(cs + 1, row), 0);
    vec4 c2 = texelFetch(u_boneTex, ivec2(cs + 2, row), 0);
    vec4 c3 = texelFetch(u_boneTex, ivec2(cs + 3, row), 0);
    return mat4(c0, c1, c2, c3);
}

void main() {
    mat4 world_mat = mat4(1.0);
    if (bone_index >= 0) {
        world_mat = fetch_matrix(bone_index);
    }
    vec4 world_pos = u_modelMat * world_mat * vec4(in_position, 1.0);
    gl_Position = u_projMat * u_viewMat * world_pos;
    v_worldPos = world_pos.xyz;
    v_normal = normalize(mat3(u_modelMat) * mat3(world_mat) * in_normal);
    v_color = in_color;
}
