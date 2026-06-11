cmake_minimum_required(VERSION 3.22.1)
project(quickstore_jni CXX)

set(QUICKSTORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../")

add_library(quickstore_jni SHARED
    jni_bridge.cpp
    ${QUICKSTORE_ROOT}core/src/codec.cpp
    ${QUICKSTORE_ROOT}core/src/crc32_validator.cpp
    ${QUICKSTORE_ROOT}core/src/data_parser.cpp
    ${QUICKSTORE_ROOT}core/src/data_validator.cpp
    ${QUICKSTORE_ROOT}core/src/file_lock.cpp
    ${QUICKSTORE_ROOT}core/src/kv_store.cpp
    ${QUICKSTORE_ROOT}core/src/meta_info.cpp
    ${QUICKSTORE_ROOT}core/src/mmap_region.cpp
    ${QUICKSTORE_ROOT}core/src/reader.cpp
    ${QUICKSTORE_ROOT}core/src/varint.cpp
    ${QUICKSTORE_ROOT}core/src/writer.cpp
)

target_include_directories(quickstore_jni PRIVATE
    ${QUICKSTORE_ROOT}core/include
)

target_compile_options(quickstore_jni PRIVATE
    -std=c++17 -fno-exceptions -fno-rtti -Wall
    $<$<CONFIG:Release>:-flto=thin>
)

target_link_options(quickstore_jni PRIVATE
    $<$<CONFIG:Release>:-flto=thin>
)

target_link_libraries(quickstore_jni PRIVATE android log z)
