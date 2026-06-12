#pragma once

// ═══════════════════════════════════════════════════════════════════════════
// Shader Standard — contract between engine GLSL shaders and C++ renderer.
//
// Every built-in / custom shader MUST follow these conventions so shaders
// are drop-in swappable without touching C++ code.
//
// Conventions:
//   Engine→shader uniforms:  u_ prefix
//   Shader parameters:       u_param_ prefix
//   Samplers:                declared in shader, bound to fixed texture units
//   Vertex attributes:       layout(location=N) in shader, matched here
//
// NOTE: The built-in shaders (resources/core/shaders/*) still use the old
// names (camera_pos, light_dir, etc.) and have NOT been migrated yet.
// Pipeline.cpp uses the old names for compatibility.  New custom shaders
// (e.g. a PBR effect) should use the u_ names defined here.
// ═══════════════════════════════════════════════════════════════════════════

// ──── Fixed texture units ────
#define TEX_UNIT_DIFFUSE   0   // PMX diffuse / material texture
#define TEX_UNIT_BONE      1   // Bone matrices (RGBA32F, for skinned variants)
#define TEX_UNIT_GRADIENT  2   // 4×1 gray gradient (toon ramp)
#define TEX_UNIT_SPHERE    3   // PMX sphere map (additive / multiplicative)
#define TEX_UNIT_TOON      4   // PMX toon texture / shared toon

// ──── Vertex attribute locations ────
// Static VAO (no skinning):           0=pos  1=norm  2=uv  3=edge
// Skinned VAO:                         0=pos  1=norm  2=uv  3=boneIdx  4=boneWt  [5=edge]
// Skinned+Morph VAO:                   0=pos  1=norm  2=uv  3=boneIdx  4=boneWt  5=morphPos  6=morphUV  [7=edge]
// Edge is only present on outline-pass VAOs.
#define ATTR_POSITION        0   // vec3  — model-space position
#define ATTR_NORMAL          1   // vec3  — model-space normal
#define ATTR_TEXCOORD        2   // vec2  — UV coordinates
#define ATTR_EDGE            3   // float — edge scale factor (outline static; =3 for skinned=5 for morph=7)
#define ATTR_BONE_INDICES    3   // ivec4 — bone indices (skinned: overrides ATTR_EDGE)
#define ATTR_BONE_WEIGHTS    4   // vec4  — bone weights
#define ATTR_MORPH_OFFSET    5   // vec3  — morph position delta
#define ATTR_UV_MORPH_OFFSET 6   // vec2  — morph UV delta
#define ATTR_EDGE_SKINNED    5   // float — edge scale (outline_skinned)
#define ATTR_EDGE_MORPH      7   // float — edge scale (outline_skinned_morph)

// ──── Engine uniforms (engine always provides; shader declares what it needs) ────
#define U_MODEL_MAT           "u_modelMat"
#define U_VIEW_MAT            "u_viewMat"
#define U_PROJ_MAT            "u_projMat"
#define U_CAMERA_POS          "u_cameraPos"
#define U_LIGHT_DIR           "u_lightDir"
#define U_MATERIAL_DIFFUSE    "u_materialDiffuse"
#define U_MATERIAL_AMBIENT    "u_materialAmbient"
#define U_MATERIAL_ALPHA      "u_materialAlpha"
#define U_HAS_TEXTURE         "u_hasTex"
#define U_BONE_TEX_WIDTH      "u_boneTexWidth"
#define U_MORPH_WEIGHT        "u_morphWeight"
#define U_OUTLINE_THICKNESS   "u_outlineThickness"
#define U_OUTLINE_COLOR       "u_outlineColor"

// ──── Toon parameters (set when toon mode is active) ────
#define U_PARAM_SHADOW_THRESH "u_param_shadowThresh"
#define U_PARAM_RIM_POWER     "u_param_rimPower"
#define U_PARAM_RIM_COLOR     "u_param_rimColor"
#define U_PARAM_SPHERE_MODE   "u_param_sphereMode"
#define U_PARAM_HAS_TOON      "u_param_hasToon"

