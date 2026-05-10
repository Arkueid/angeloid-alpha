#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_position;
in vec3 v_world_pos;

uniform vec3 light_dir;
uniform vec3 camera_pos;
uniform sampler2D tex;
uniform bool has_texture;
uniform sampler2D gradient_map;
uniform float alpha;
uniform vec3 material_color;

uniform float shadow_thresh;
uniform float rim_power;
uniform vec3 rim_color;

uniform sampler2D toon_tex;
uniform bool has_toon;

uniform sampler2D sphere_tex;
uniform int sphere_mode;

out vec4 fragColor;

void main() {
    if (alpha < 0.01) {
        discard;
    }

    vec3 normal = normalize(v_normal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    vec3 light = normalize(light_dir);
    vec3 view_dir = normalize(camera_pos - v_world_pos);

    float NdotL = dot(normal, light);
    float shadow = NdotL > shadow_thresh ? 1.0 : 0.0;

    vec4 tex_color;
    if (has_texture) {
        tex_color = texture(tex, v_uv);
        if (tex_color.a < 0.1) {
            discard;
        }
    } else {
        tex_color = vec4(material_color, 1.0);
    }

    vec3 base_color = tex_color.rgb;

    vec3 shaded_color;
    if (has_toon) {
        float toon_uv_y = (1.0 - NdotL) * 0.5;
        vec3 toon_color = texture(toon_tex, vec2(0.5, toon_uv_y)).rgb;
        shaded_color = base_color * toon_color;
    } else {
        shaded_color = base_color * (0.6 + NdotL * 0.4);
    }

    vec2 sphere_uv = normal.xy * 0.5 + 0.5;
    if (sphere_mode == 1) {
        vec3 sphere_color = texture(sphere_tex, sphere_uv).rgb;
        shaded_color = shaded_color * sphere_color;
    } else if (sphere_mode == 2) {
        vec3 sphere_color = texture(sphere_tex, sphere_uv).rgb;
        shaded_color = shaded_color + sphere_color * 0.5;
    } else if (sphere_mode == 3) {
        vec3 sphere_color = texture(sphere_tex, v_uv).rgb;
        shaded_color = shaded_color * sphere_color;
    }

    float rim = pow(1.0 - max(dot(view_dir, normal), 0.0), rim_power);
    vec3 rim_contribution = rim * rim_color * 0.5;

    vec3 result = shaded_color + rim_contribution;
    result = clamp(result, 0.0, 1.0);

    fragColor = vec4(result, tex_color.a * alpha);
}
