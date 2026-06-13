cmake_minimum_required(VERSION 3.20)
project(mmd LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MMD_SOURCES
    core/pmx/PmxReader.cpp
    core/anim/BoneSkinning.cpp
    core/anim/MorphController.cpp
    core/anim/PhysicsWorld.cpp
    core/anim/VmdPlayer.cpp
    core/anim/VpdLoader.cpp
    core/encoding/Encoding.cpp
    Model.cpp
    framework/opengl/gpu/Mesh.cpp
    framework/opengl/gpu/Texture.cpp
    framework/opengl/gpu/Shader.cpp
    framework/opengl/ModelRenderer.cpp
    framework/opengl/RenderContext.cpp
    framework/opengl/RenderTarget.cpp
    framework/opengl/debug/RigidBodyRenderer.cpp
    framework/opengl/Pipeline.cpp
    framework/opengl/StbImage.cpp
    framework/Camera.cpp
    framework/MMD.cpp
)

# Build glad (required by angeloid)
add_library(glad ${CMAKE_SOURCE_DIR}/thirdparty/glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/glad/include)

# Build bullet (required by angeloid)
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET3 OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/bullet bullet)

add_library(angeloid STATIC ${MMD_SOURCES})
add_library(angeloid::core ALIAS angeloid)

target_include_directories(angeloid PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/bullet/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/glad/include
)

target_link_libraries(angeloid PUBLIC
    glad
    BulletDynamics BulletCollision LinearMath
)

if(ENABLE_STACKTRACE)
    target_link_libraries(angeloid PUBLIC Backward::Backward)
    target_compile_definitions(angeloid PUBLIC MMD_ENABLE_STACKTRACE)
endif()
