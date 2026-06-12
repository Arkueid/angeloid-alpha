#version 330 core

in vec3 v_normal;
in vec2 v_uv;
in vec3 v_position;
in vec3 v_world_pos;

uniform vec3 light_dir;
uniform sampler2D tex;
uniform bool has_texture;
uniform float alpha;
uniform vec3 material_color;

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

    float diff = max(dot(normal, light), 0.0);
    float ambient = 0.6;

    vec3 color;
    if (has_texture) {
        vec4 tex_color = texture(tex, v_uv);
        color = tex_color.rgb;
        if (tex_color.a < 0.1) {
            discard;
        }
    } else {
        color = material_color;
    }

    vec3 result = color * (ambient + diff * 0.4);
    result = clamp(result, 0.0, 1.0);
    fragColor = vec4(result, alpha);
}
