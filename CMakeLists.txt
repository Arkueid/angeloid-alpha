cmake_minimum_required(VERSION 3.20)
project(angeloid-alpha LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use static CRT to match Bullet's USE_MSVC_RUNTIME_LIBRARY_DLL=OFF
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(mmd)
add_subdirectory(viewer)
