#version 450

layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_color;
layout(location=2) in int bone_index;
layout(location=3) in vec3 in_normal;

layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_modelMat;
    int u_boneTexWidth;
} _ub;

layout(set=0, binding=2) uniform sampler2D u_boneTex;

layout(location=0) out vec3 v_color;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec3 v_worldPos;

mat4 fetch_matrix(int id) {
    int tpm = 4;
    int gti = id * tpm;
    int row = gti / _ub.u_boneTexWidth;
    int cs = gti % _ub.u_boneTexWidth;
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
    vec4 world_pos = _ub.u_modelMat * world_mat * vec4(in_position, 1.0);
    gl_Position = _ub.u_projMat * _ub.u_viewMat * world_pos;
    v_worldPos = world_pos.xyz;
    v_normal = normalize(mat3(_ub.u_modelMat) * mat3(world_mat) * in_normal);
    v_color = in_color;
}
