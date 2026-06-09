cmake_minimum_required(VERSION 3.22.1)
project(quickstore_jni CXX)

set(QUICKSTORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../")
file(GLOB QUICKSTORE_CORE_SOURCES "${QUICKSTORE_ROOT}core/src/*.cpp")

add_library(quickstore_jni SHARED
    jni_bridge.cpp
    ${QUICKSTORE_CORE_SOURCES}
)

target_include_directories(quickstore_jni PRIVATE
    ${QUICKSTORE_ROOT}core/include
)

target_compile_options(quickstore_jni PRIVATE
    -std=c++17 -fno-exceptions -fno-rtti -Wall
)

target_link_libraries(quickstore_jni PRIVATE android log z)
