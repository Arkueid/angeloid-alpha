#version 330 core

in vec3 v_world_pos;

uniform sampler2D u_shadowMap;
uniform mat4 u_lightViewProj;
uniform bool u_hasShadow;

out vec4 fragColor;

void main() {
    float shadow = 1.0;
    if (u_hasShadow) {
        vec4 lightClip = u_lightViewProj * vec4(v_world_pos, 1.0);
        vec3 lightNdc = lightClip.xyz / lightClip.w;
        if (lightNdc.x > -1.0 && lightNdc.x < 1.0 &&
            lightNdc.y > -1.0 && lightNdc.y < 1.0) {
            vec2 uv = lightNdc.xy * 0.5 + 0.5;
            float shadowDepth = texture(u_shadowMap, uv).r;
            float fragDepth = lightNdc.z * 0.5 + 0.5;
            shadow = fragDepth - 0.003 <= shadowDepth ? 1.0 : 0.3;
        }
    }

    // Checkerboard pattern for ground visibility
    float cx = floor(v_world_pos.x);
    float cz = floor(v_world_pos.z);
    float checker = mod(cx + cz, 2.0) < 1.0 ? 0.6 : 0.4;
    vec3 groundColor = vec3(checker) * shadow;

    // Offset strips for spatial reference
    float stripe = abs(fract(v_world_pos.z * 0.25) - 0.5) * 2.0; // 0~1 sawtooth
    float lineMask = smoothstep(0.95, 1.0, stripe);
    groundColor = mix(groundColor, vec3(0.2), lineMask * 0.5);

    fragColor = vec4(groundColor, 1.0);
}
