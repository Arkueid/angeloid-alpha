cmake_minimum_required(VERSION 3.20)
project(angeloid-alpha LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static CRT to match Bullet's USE_MSVC_RUNTIME_LIBRARY_DLL=OFF
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    # glew.h redefines APIENTRY (from windows.h); u8string char8_t vs printf %s
    add_compile_options(/wd4005 /wd4477)
endif()

set(ENABLE_STACKTRACE ON CACHE BOOL "Enable crash stack trace via backward-cpp" FORCE)
set(BUILD_VIEWER OFF CACHE BOOL "Build viewer")

if(ENABLE_STACKTRACE)
    add_subdirectory(thirdparty/backward-cpp)
endif()

add_subdirectory(mmd)

if(BUILD_VIEWER)
    add_subdirectory(viewer)
endif()
