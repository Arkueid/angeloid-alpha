#pragma once

// Shader Standard — contract between engine GLSL shaders and C++ renderer.
// Every shader MUST follow these conventions so shaders are drop-in swappable.

// ──── Fixed texture units ────
#define TEX_UNIT_DIFFUSE   0
#define TEX_UNIT_BONE      1
#define TEX_UNIT_GRADIENT  2
#define TEX_UNIT_SPHERE    3
#define TEX_UNIT_TOON      4

// ──── Vertex attribute locations ────
#define ATTR_POSITION        0
#define ATTR_NORMAL          1
#define ATTR_TEXCOORD        2
#define ATTR_BONE_INDICES    3
#define ATTR_BONE_WEIGHTS    4
#define ATTR_MORPH_OFFSET    5
#define ATTR_UV_MORPH_OFFSET 6
#define ATTR_EDGE            7

// ──── Engine uniforms ────
#define U_MODEL_MAT           "u_modelMat"
#define U_VIEW_MAT            "u_viewMat"
#define U_PROJ_MAT            "u_projMat"
#define U_CAMERA_POS          "u_cameraPos"
#define U_LIGHT_DIR           "u_lightDir"
#define U_MATERIAL_DIFFUSE    "u_materialDiffuse"
#define U_MATERIAL_AMBIENT    "u_materialAmbient"
#define U_MATERIAL_ALPHA      "u_materialAlpha"
#define U_SPECULAR_COLOR      "u_specularColor"
#define U_SPECULAR_FACTOR     "u_specularFactor"
#define U_HAS_TEXTURE         "u_hasTex"
#define U_BONE_TEX_WIDTH      "u_boneTexWidth"
#define U_MORPH_WEIGHT        "u_morphWeight"
#define U_OUTLINE_THICKNESS   "u_outlineThickness"
#define U_OUTLINE_COLOR       "u_outlineColor"

// ──── Toon parameters ────
#define U_SHADOW_THRESH       "u_shadowThresh"
#define U_RIM_POWER           "u_rimPower"
#define U_RIM_COLOR           "u_rimColor"
#define U_SPHERE_MODE         "u_sphereMode"
#define U_HAS_TOON            "u_hasToon"
