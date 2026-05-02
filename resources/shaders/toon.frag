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

uniform float shadow_thresh;
uniform float rim_power;
uniform vec3 rim_color;

out vec4 fragColor;

void main() {
    vec3 normal = normalize(v_normal);
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
        float height_shade = (v_position.y + 10.0) / 20.0;
        tex_color = vec4(0.9 * height_shade + 0.3, 0.7 * height_shade + 0.2, 0.6 * height_shade + 0.2, 1.0);
    }

    vec3 base_color = tex_color.rgb;

    float gradient_lookup = shadow * 0.5 + 0.2;
    vec3 gradient_color = texture(gradient_map, vec2(gradient_lookup, 0.5)).rgb;

    vec3 shaded_color = base_color * gradient_color;

    float rim = pow(1.0 - max(dot(view_dir, normal), 0.0), rim_power);
    vec3 rim_contribution = rim * rim_color * 0.5;

    vec3 result = shaded_color + rim_contribution;
    result = clamp(result, 0.0, 1.0);

    fragColor = vec4(result, tex_color.a);
}
