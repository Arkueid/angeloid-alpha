cmake_minimum_required(VERSION 3.20)
project(mmd LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(MMD_SOURCES
    pmx/PmxReader.cpp
    anim/BoneSkinning.cpp
    anim/MorphController.cpp
    anim/PhysicsWorld.cpp
    anim/VmdPlayer.cpp
    anim/VpdLoader.cpp
    encoding/Encoding.cpp
    render/opengl/gpu/Mesh.cpp
    render/opengl/gpu/Texture.cpp
    render/opengl/gpu/Shader.cpp
    render/opengl/ModelRenderer.cpp
    render/opengl/RenderContext.cpp
    render/opengl/debug/RigidBodyRenderer.cpp
    render/opengl/ShaderManager.cpp
    render/opengl/StbImage.cpp
    Camera.cpp
    MMD.cpp
    Model.cpp
)

add_library(mmd STATIC ${MMD_SOURCES})
if(BUILD_PYTHON_WRAPPER)
    set_target_properties(mmd PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
add_library(angeloid::core ALIAS mmd)

target_include_directories(mmd PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/bullet/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/glad/include
)

target_link_libraries(mmd PUBLIC
    glad
    BulletDynamics BulletCollision LinearMath
)

if(ENABLE_STACKTRACE)
    target_link_libraries(mmd PUBLIC Backward::Backward)
    target_compile_definitions(mmd PUBLIC MMD_ENABLE_STACKTRACE)
endif()
