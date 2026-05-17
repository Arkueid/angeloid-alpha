cmake_minimum_required(VERSION 3.20)

project(mmd LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC: accept UTF-8 source files without BOM
if (MSVC)
    add_compile_options(/utf-8)
endif()

# --- GLFW ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)

# --- Glad (OpenGL loader) ---
add_library(glad
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Glad/src/glad.c
)
target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Glad/include
)

# --- Core library (shared between main and tests) ---
set(CORE_SOURCES
    src/core/Application.cpp
    src/core/Camera.cpp
    src/core/Encoding.cpp
    src/pmx/PmxReader.cpp
    src/gpu/Mesh.cpp
    src/gpu/Texture.cpp
    src/gpu/Shader.cpp
    src/anim/BoneSkinning.cpp
    src/anim/MorphController.cpp
    src/anim/VmdPlayer.cpp
    src/anim/VpdLoader.cpp
    src/render/ModelRenderer.cpp
    src/render/PhysicsDebug.cpp
    src/render/ShaderManager.cpp
    src/render/WorldAxis.cpp
    src/StbImage.cpp
)

add_library(mmdCore STATIC ${CORE_SOURCES})

target_include_directories(mmdCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb
)

target_link_libraries(mmdCore PUBLIC
    glfw
    glad
)

# --- Main executable ---
add_executable(mmd main.cpp)
target_link_libraries(mmd PRIVATE mmdCore)
target_compile_definitions(mmd PRIVATE
    MMD_PROJECT_ROOT="${CMAKE_SOURCE_DIR}/../"
)

