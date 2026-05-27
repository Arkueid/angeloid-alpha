cmake_minimum_required(VERSION 3.20)
project(angeloid-alpha LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(mmd)
add_subdirectory(viewer)
