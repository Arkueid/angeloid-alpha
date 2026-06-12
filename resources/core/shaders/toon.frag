#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_position;
in vec3 v_world_pos;

uniform vec3 u_lightDir;
uniform vec3 u_cameraPos;
uniform sampler2D u_tex;
uniform bool u_hasTex;
uniform sampler2D u_gradientMap;
uniform float u_materialAlpha;
uniform vec3 u_materialDiffuse;

uniform float u_shadowThresh;
uniform float u_rimPower;
uniform vec3 u_rimColor;

uniform sampler2D u_toonTex;
uniform bool u_hasToon;

uniform sampler2D u_sphereTex;
uniform int u_sphereMode;

out vec4 fragColor;

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

    float NdotL = dot(normal, light);

    vec4 tex_color;
    if (u_hasTex) {
        tex_color = texture(u_tex, v_uv);
        if (tex_color.a < 0.1) {
            discard;
        }
    } else {
        tex_color = vec4(u_materialDiffuse, 1.0);
    }

    vec3 base_color = tex_color.rgb;

    vec3 shaded_color;
    if (u_hasToon) {
        float toon_uv_y = (1.0 - NdotL) * 0.5;
        vec3 toon_color = texture(u_toonTex, vec2(0.5, toon_uv_y)).rgb;
        shaded_color = base_color * toon_color;
    } else {
        shaded_color = base_color * (0.6 + NdotL * 0.4);
    }

    vec2 sphere_uv = normal.xy * 0.5 + 0.5;
    if (u_sphereMode == 1) {
        vec3 sphere_color = texture(u_sphereTex, sphere_uv).rgb;
        shaded_color = shaded_color * sphere_color;
    } else if (u_sphereMode == 2) {
        vec3 sphere_color = texture(u_sphereTex, sphere_uv).rgb;
        shaded_color = shaded_color + sphere_color * 0.5;
    } else if (u_sphereMode == 3) {
        vec3 sphere_color = texture(u_sphereTex, v_uv).rgb;
        shaded_color = shaded_color * sphere_color;
    }

    float rim = pow(1.0 - max(dot(view_dir, normal), 0.0), u_rimPower);
    vec3 rim_contribution = rim * u_rimColor * 0.5;

    vec3 result = shaded_color + rim_contribution;
    result = clamp(result, 0.0, 1.0);

    fragColor = vec4(result, tex_color.a * u_materialAlpha);
}
