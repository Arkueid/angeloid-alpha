#version 330 core

in vec3 v_world_pos;

uniform sampler2DShadow u_shadowMap;
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
            float fragDepth = lightNdc.z * 0.5 + 0.5;
            // Clamp: fragments beyond far plane would incorrectly compare against
            // the cleared depth (1.0) and fail GL_LEQUAL, causing false shadow
            fragDepth = clamp(fragDepth, 0.0, 1.0);

            float bias = 0.002;
            // Hardware 4-tap PCF (sampler2DShadow + GL_LINEAR filtering)
            float pcf = texture(u_shadowMap, vec3(uv, fragDepth - bias));
            shadow = 0.5 + pcf * 0.5;
        }
    }

    vec3 groundColor = vec3(1.0) * shadow;
    fragColor = vec4(groundColor, 1.0);
}
