include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

set(QUICKSTORE_TEST_TARGETS
    test_varint
    test_codec
    test_meta_info
    test_decode_map
    test_crc32
    test_data_validator
    test_varint_write
    test_encode
    test_crc32_write
    test_meta_write
    test_writer_roundtrip
    test_writer_full_writeback
    test_writer_meta
    test_kv_store_cabi
    test_file_lock
)

foreach(target ${QUICKSTORE_TEST_TARGETS})
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE quickstore_core GTest::gtest_main)
    target_compile_features(${target} PRIVATE cxx_std_17)
    gtest_discover_tests(${target})
endforeach()

# Multi-process integration tests — fork-based, Linux/macOS only (not iOS)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(test_multiprocess test_multiprocess.cpp)
    target_link_libraries(test_multiprocess PRIVATE quickstore_core GTest::gtest_main)
    target_compile_features(test_multiprocess PRIVATE cxx_std_17)
    gtest_discover_tests(test_multiprocess)
endif()

# Golden file integration tests — also needs GMock for UnorderedElementsAre
add_executable(test_golden_files test_golden_files.cpp)
target_link_libraries(test_golden_files PRIVATE
    quickstore_core
    GTest::gtest_main
    GTest::gmock_main
)
target_compile_features(test_golden_files PRIVATE cxx_std_17)
target_compile_definitions(test_golden_files PRIVATE
    QUICKSTORE_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
)
gtest_discover_tests(test_golden_files)

# Crash-consistency harness — fork-based, Linux/macOS only (not iOS)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    add_executable(test_crash_harness test_crash_harness.cpp)
    target_link_libraries(test_crash_harness PRIVATE quickstore_core GTest::gtest_main)
    target_compile_features(test_crash_harness PRIVATE cxx_std_17)
    gtest_discover_tests(test_crash_harness)
endif()

# Thread-safety stress tests — always built; flags inherited from quickstore_core
# when QUICKSTORE_ENABLE_TSAN=ON. Uses std::thread (no fork, no iOS restriction).
add_executable(test_tsan_stress test_tsan_stress.cpp)
target_link_libraries(test_tsan_stress PRIVATE quickstore_core GTest::gtest_main)
target_compile_features(test_tsan_stress PRIVATE cxx_std_17)
gtest_discover_tests(test_tsan_stress)

# libFuzzer harness — Clang/AppleClang only; EXCLUDE_FROM_ALL so it is NOT
# built by the default 'cmake --build' invocation. Build it explicitly:
#   cmake --build <dir> --target fuzz_decoder
# Run: ./fuzz_decoder -runs=1000
# Note: requires the fuzzer runtime (libclang_rt.fuzzer_osx.a) to be available
# in the active toolchain. AppleClang at /usr/bin/c++ may lack it; the Swift
# toolchain at ~/.swiftly/bin/clang++ provides it. Use the appropriate toolchain.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    add_executable(fuzz_decoder EXCLUDE_FROM_ALL fuzz_decoder.cpp)
    target_compile_options(fuzz_decoder PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_decoder PRIVATE -fsanitize=fuzzer,address)
    target_link_libraries(fuzz_decoder PRIVATE quickstore_core)
    target_compile_features(fuzz_decoder PRIVATE cxx_std_17)
    # NOT added to gtest_discover_tests or add_test — run manually only
endif()

# Google Benchmark target — only when QUICKSTORE_BUILD_BENCHMARKS=ON.
# NOT registered as a ctest target; run manually: ./bench_kvstore
if(QUICKSTORE_BUILD_BENCHMARKS)
    add_executable(bench_kvstore bench_kvstore.cpp)
    target_link_libraries(bench_kvstore PRIVATE quickstore_core benchmark::benchmark)
    target_compile_features(bench_kvstore PRIVATE cxx_std_17)
    # NOT added to gtest_discover_tests or add_test
endif()
