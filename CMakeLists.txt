cmake_minimum_required(VERSION 3.21)
project(quick-store VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(NOT ANDROID)
    find_package(ZLIB REQUIRED)
endif()

enable_testing()
add_subdirectory(core)
