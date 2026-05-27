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
)

add_library(mmd STATIC ${MMD_SOURCES})

target_include_directories(mmd PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/bullet/src
)
