#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_position;
in vec3 v_world_pos;

uniform vec3 u_lightDir;
uniform vec3 u_cameraPos;
uniform sampler2D u_tex;
uniform bool u_hasTex;
uniform float u_materialAlpha;
uniform vec3 u_materialDiffuse;
uniform vec3 u_materialAmbient;
uniform vec3 u_specularColor;
uniform float u_specularFactor;

uniform sampler2DShadow u_shadowMap;
uniform mat4 u_lightViewProj;
uniform bool u_hasShadow;

out vec4 fragColor;

float shadowFactor() {
    if (!u_hasShadow) return 1.0;
    vec4 lightClip = u_lightViewProj * vec4(v_world_pos, 1.0);
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    if (lightNdc.x < -1.0 || lightNdc.x > 1.0 ||
        lightNdc.y < -1.0 || lightNdc.y > 1.0)
        return 1.0;

    vec2 uv = lightNdc.xy * 0.5 + 0.5;
    float fragDepth = lightNdc.z * 0.5 + 0.5;
    fragDepth = clamp(fragDepth, 0.0, 1.0);

    float bias = 0.002;
    // Hardware 4-tap PCF (sampler2DShadow + bilinear)
    float pcf = texture(u_shadowMap, vec4(uv, 0.0, fragDepth - bias));
    return 0.5 + pcf * 0.5;
}

void main() {
    if (u_materialAlpha < 0.01) {
        discard;
    }

    vec3 normal = normalize(v_normal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    vec3 light = normalize(u_lightDir);
    vec3 view_dir = normalize(u_cameraPos - v_world_pos);

    float diff = max(dot(normal, light), 0.0);

    vec3 color;
    if (u_hasTex) {
        vec4 tex_color = texture(u_tex, v_uv);
        color = tex_color.rgb;
        if (tex_color.a < 0.1) {
            discard;
        }
    } else {
        color = u_materialDiffuse;
    }

    vec3 ambient_term = color * u_materialAmbient;
    vec3 diffuse_term = color * diff * 0.4;

    // Specular (Blinn-Phong)
    vec3 half_vec = normalize(light + view_dir);
    float spec = pow(max(dot(normal, half_vec), 0.0), u_specularFactor);
    vec3 specular = spec * u_specularColor;

    float shadow = shadowFactor();
    vec3 result = ambient_term + (diffuse_term + specular) * shadow;
    result = clamp(result, 0.0, 1.0);
    fragColor = vec4(result, u_materialAlpha);
}
