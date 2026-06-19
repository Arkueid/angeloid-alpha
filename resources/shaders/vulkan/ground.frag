#version 450

layout(location=0) in vec3 v_world_pos;

layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_lightViewProj;
    int u_hasShadow;
} _ub;

layout(set=0, binding=6) uniform sampler2DShadow u_shadowMap;

layout(location=0) out vec4 fragColor;

void main() {
    float shadow = 1.0;
    if (_ub.u_hasShadow != 0) {
        vec4 lightClip = _ub.u_lightViewProj * vec4(v_world_pos, 1.0);
        vec3 lightNdc = lightClip.xyz / lightClip.w;
        if (lightNdc.x > -1.0 && lightNdc.x < 1.0 &&
            lightNdc.y > -1.0 && lightNdc.y < 1.0) {
            vec2 uv = vec2(lightNdc.x * 0.5 + 0.5, lightNdc.y * -0.5 + 0.5);
            float fragDepth = clamp(lightNdc.z, 0.0, 1.0);  // already in [0,1] from depth-corrected proj

            float bias = 0.002;
            float pcf = texture(u_shadowMap, vec3(uv, fragDepth - bias));
            shadow = 0.5 + pcf * 0.5;
        }
    }

    vec3 groundColor = vec3(1.0) * shadow;
    fragColor = vec4(groundColor, 1.0);
}
