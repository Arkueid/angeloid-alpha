#version 450

layout(location=0) in vec3 v_normal;
layout(location=1) in vec2 v_uv;
layout(location=2) in vec3 v_position;
layout(location=3) in vec3 v_world_pos;

// Same combined block as main.vert
layout(set=0, binding=0) uniform _UniformBlock {
    mat4 u_projMat;
    mat4 u_viewMat;
    mat4 u_modelMat;
    int u_boneTexWidth;
    float u_morphWeight;
    vec3 u_lightDir;
    vec3 u_cameraPos;
    int u_hasTex;
    float u_materialAlpha;
    vec3 u_materialDiffuse;
    vec3 u_materialAmbient;
    vec3 u_specularColor;
    float u_specularFactor;
    float u_rimPower;
    vec3 u_rimColor;
    int u_hasToon;
    int u_sphereMode;
    mat4 u_lightViewProj;
    int u_hasShadow;
} _ub;

layout(set=0, binding=1) uniform sampler2D u_tex;
layout(set=0, binding=6) uniform sampler2DShadow u_shadowMap;

layout(location=0) out vec4 fragColor;

float shadowFactor() {
    if (_ub.u_hasShadow == 0) return 1.0;
    vec4 lightClip = _ub.u_lightViewProj * vec4(v_world_pos, 1.0);
    vec3 lightNdc = lightClip.xyz / lightClip.w;
    if (lightNdc.x < -1.0 || lightNdc.x > 1.0 ||
        lightNdc.y < -1.0 || lightNdc.y > 1.0)
        return 1.0;

    vec2 uv = vec2(lightNdc.x * 0.5 + 0.5, lightNdc.y * -0.5 + 0.5);
    float fragDepth = clamp(lightNdc.z, 0.0, 1.0);  // already in [0,1] from depth-corrected proj

    float NdotL = abs(dot(normalize(v_normal), normalize(_ub.u_lightDir)));
    float bias = max(0.005 * (1.0 - NdotL), 0.001);
    float pcf = texture(u_shadowMap, vec3(uv, fragDepth - bias));
    return 0.5 + pcf * 0.5;
}

void main() {
    if (_ub.u_materialAlpha < 0.01) {
        discard;
    }

    vec3 normal = normalize(v_normal);
    if (!gl_FrontFacing) {
        normal = -normal;
    }
    vec3 light = normalize(_ub.u_lightDir);
    vec3 view_dir = normalize(_ub.u_cameraPos - v_world_pos);

    float diff = max(dot(normal, light), 0.0);

    vec3 color;
    if (_ub.u_hasTex != 0) {
        vec4 tex_color = texture(u_tex, v_uv);
        color = tex_color.rgb;
        if (tex_color.a < 0.1) {
            discard;
        }
    } else {
        color = _ub.u_materialDiffuse;
    }

    vec3 ambient_term = color * _ub.u_materialAmbient;
    vec3 diffuse_term = color * diff * 0.4;

    vec3 half_vec = normalize(light + view_dir);
    float spec = pow(max(dot(normal, half_vec), 0.0), _ub.u_specularFactor);
    vec3 specular = spec * _ub.u_specularColor;

    float shadow = shadowFactor();
    vec3 result = ambient_term + (diffuse_term + specular) * shadow;
    result = clamp(result, 0.0, 1.0);
    fragColor = vec4(result, _ub.u_materialAlpha);
}
