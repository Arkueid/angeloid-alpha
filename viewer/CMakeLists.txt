cmake_minimum_required(VERSION 3.20)

project(viewer LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

# Third-party libs (shared with angeloid target)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/glfw glfw)

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

add_executable(viewer
    main.cpp
    window/GlfwWindow.cpp
    debug/WorldAxis.cpp
)

target_include_directories(viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../angeloid
)

target_link_libraries(viewer PRIVATE angeloid glfw)
target_compile_definitions(viewer PRIVATE
    MMD_PROJECT_ROOT="${CMAKE_SOURCE_DIR}/"
)
