# ---- angeloid Wrapper ----
cmake_minimum_required(VERSION 3.20)
project(wrapper LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(Wrapper.cmake)

# Build glad (required by mmd)
add_library(glad ${CMAKE_SOURCE_DIR}/thirdparty/glad/src/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/glad/include)

# Build bullet (required by mmd)
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

set(MMD_WRAPPER_TARGET wrapper)

add_library(${MMD_WRAPPER_TARGET} SHARED
    Init.cpp
    PyModel.cpp
    PyCamera.cpp
)

target_include_directories(${MMD_WRAPPER_TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/mmd
    ${CMAKE_SOURCE_DIR}/thirdparty/glad/include
)

target_link_libraries(${MMD_WRAPPER_TARGET} PRIVATE
    angeloid::core
    Python3::SABIModule
)

set_wrapper_output(${MMD_WRAPPER_TARGET} "_angeloid")

if(MSVC)
    set_target_properties(${MMD_WRAPPER_TARGET} PROPERTIES
        DEBUG_POSTFIX ""
        RELEASE_POSTFIX ""
        RELWITHDEBINFO_POSTFIX ""
        MINSIZEREL_POSTFIX ""
    )
endif()

add_library(angeloid::wrapper ALIAS ${MMD_WRAPPER_TARGET})

add_custom_command(
    TARGET ${MMD_WRAPPER_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/package/angeloid
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${MMD_WRAPPER_TARGET}> ${CMAKE_SOURCE_DIR}/package/angeloid/
)
