option(QUICKSTORE_BUILD_TESTS "Build quickstore unit tests" ON)

# Thread Sanitizer — OFF by default. Enable in a SEPARATE build directory
# because TSan and ASan are mutually exclusive (cannot be combined in one binary).
# Usage: cmake -DQUICKSTORE_ENABLE_TSAN=ON -B build_tsan && cmake --build build_tsan
option(QUICKSTORE_ENABLE_TSAN "Enable Thread Sanitizer" OFF)

# Google Benchmark targets — OFF by default (fetches external dependency when ON).
# Usage: cmake -DQUICKSTORE_BUILD_BENCHMARKS=ON -B build_bench && cmake --build build_bench --target bench_kvstore
option(QUICKSTORE_BUILD_BENCHMARKS "Build Google Benchmark targets" OFF)

add_library(quickstore_core STATIC
    src/varint.cpp
    src/codec.cpp
    src/meta_info.cpp
    src/mmap_region.cpp
    src/data_parser.cpp
    src/crc32_validator.cpp
    src/data_validator.cpp
    src/reader.cpp
    src/writer.cpp
    src/kv_store.cpp
    src/file_lock.cpp
)

target_include_directories(quickstore_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(NOT ANDROID)
    target_link_libraries(quickstore_core PUBLIC ZLIB::ZLIB)
endif()

set_target_properties(quickstore_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

target_compile_features(quickstore_core PUBLIC cxx_std_17)

target_compile_options(quickstore_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions -fno-rtti -Wall -Wextra>
)

# TSan flags — applied PUBLIC so test executables inherit them automatically.
# IMPORTANT: TSan and ASan are mutually exclusive. Never combine in the same build.
# iOS is excluded: TSan is not supported on the iOS simulator or device.
if(QUICKSTORE_ENABLE_TSAN AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(WARNING "QUICKSTORE_ENABLE_TSAN=ON: using a SEPARATE build directory is "
                    "required. TSan and ASan are mutually exclusive — do NOT mix them "
                    "in the same binary. AppleClang TSan availability depends on macOS version.")
    target_compile_options(quickstore_core PUBLIC -fsanitize=thread -g -O1)
    target_link_options(quickstore_core PUBLIC -fsanitize=thread)
endif()

# Google Benchmark — fetched only when QUICKSTORE_BUILD_BENCHMARKS=ON.
# BENCHMARK_ENABLE_EXCEPTIONS=OFF is load-bearing: this project uses -fno-exceptions.
if(QUICKSTORE_BUILD_BENCHMARKS)
    include(FetchContent)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_USE_BUNDLED_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.8.3
    )
    FetchContent_MakeAvailable(benchmark)
endif()

if(QUICKSTORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
