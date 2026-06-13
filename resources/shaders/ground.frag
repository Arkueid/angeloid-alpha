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
            // 3x3 manual PCF (9 taps)
            vec2 texelSize = 1.0 / vec2(textureSize(u_shadowMap, 0));
            float pcf = 0.0;
            for (int x = -1; x <= 1; ++x)
                for (int y = -1; y <= 1; ++y)
                    pcf += texture(u_shadowMap, vec3(uv + vec2(x, y) * texelSize, fragDepth - 0.003));
            pcf /= 9.0;
            shadow = 0.3 + pcf * 0.7;
        }
    }

    vec3 groundColor = vec3(1.0) * shadow;
    fragColor = vec4(groundColor, 1.0);
}
