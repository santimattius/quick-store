cmake_minimum_required(VERSION 3.21)
project(quick-store-golden-generator LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build MMKV v2.4.0 for POSIX first, then point -DMMKV_DIR to its build dir.
find_package(MMKV QUIET HINTS ${MMKV_DIR})
if(NOT MMKV_FOUND)
    message(FATAL_ERROR
        "MMKV not found.\n"
        "  1. git clone --branch v2.4.0 https://github.com/Tencent/MMKV.git /tmp/mmkv\n"
        "  2. cd /tmp/mmkv/POSIX/build && cmake .. && make -j\n"
        "  3. cmake -DMMKV_DIR=/tmp/mmkv/POSIX/build .."
    )
endif()

add_executable(generate_golden generate_golden.cpp)
target_link_libraries(generate_golden PRIVATE MMKV)
