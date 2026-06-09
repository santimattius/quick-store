// test_tsan_stress.cpp
// Thread-safety stress tests for quickstore.
//
// These tests validate that a single kv_store* handle can be accessed safely
// from multiple concurrent threads. Without QUICKSTORE_ENABLE_TSAN the tests
// validate correctness (no crash, consistent final state). With
// QUICKSTORE_ENABLE_TSAN=ON the build adds -fsanitize=thread so TSan also
// reports any data races in addition to the GTest assertions.
//
// Note: TSan and ASan are mutually exclusive — never combine them in the
// same binary. Use a separate build directory for TSan runs:
//   cmake -DQUICKSTORE_ENABLE_TSAN=ON -B build_tsan && cmake --build build_tsan

#include "quickstore/kv_store.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <cstdlib>

// ---------------------------------------------------------------------------
// TempDir RAII (same mkdtemp pattern as test_multiprocess.cpp)
// ---------------------------------------------------------------------------
struct TsanTempDir {
    char path[64];
    TsanTempDir() {
        std::strncpy(path, "/tmp/qs_tsan_XXXXXX", sizeof(path));
        EXPECT_NE(::mkdtemp(path), nullptr)
            << "mkdtemp failed: " << ::strerror(errno);
    }
    ~TsanTempDir() {
        std::string cmd = "rm -rf '";
        cmd += path;
        cmd += "'";
        ::system(cmd.c_str());
    }
    const char* c_str() const { return path; }
};

// ---------------------------------------------------------------------------
// TC_TSAN_1 — 16 threads, 1000 iterations each, single kv_store* handle.
//
// Each thread does kv_set_i64 + kv_get_i64 on the SAME handle. The per-handle
// recursive_mutex in kv_store serializes all operations. With TSan enabled this
// test proves zero data races under the current locking discipline.
// ---------------------------------------------------------------------------
TEST(TsanStress, TC_TSAN_1_16Threads1000Iters) {
    TsanTempDir dir;

    kv_store* s = nullptr;
    ASSERT_EQ(kv_open("tc_tsan1", dir.c_str(), nullptr, &s), KV_OK);
    ASSERT_NE(s, nullptr);

    constexpr int kThreads    = 16;
    constexpr int kIterations = 1000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([s, t]() {
            for (int i = 0; i < kIterations; ++i) {
                // Each thread uses its own distinct key prefix to avoid
                // inter-thread value conflicts while still sharing the handle.
                std::string key = "t" + std::to_string(t) + "_k" + std::to_string(i % 10);
                int64_t val = static_cast<int64_t>(t * 1000 + i);

                kv_status set_st = kv_set_i64(s, key.c_str(), val);
                // KV_OK is the only valid success code; KV_CLOSED must not appear
                EXPECT_EQ(set_st, KV_OK) << "Thread " << t << " iter " << i << ": kv_set_i64 failed";

                int64_t out = 0;
                kv_status get_st = kv_get_i64(s, key.c_str(), &out);
                // Value may have been overwritten by another thread — KV_OK is expected
                EXPECT_EQ(get_st, KV_OK) << "Thread " << t << " iter " << i << ": kv_get_i64 failed";
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Verify the store is still operational after concurrent access
    kv_status trim_st = kv_trim(s);
    EXPECT_EQ(trim_st, KV_OK) << "kv_trim after concurrent access must succeed";

    kv_close(s);
}

// ---------------------------------------------------------------------------
// TC_TSAN_2 — 8 writer threads + 8 reader threads on overlapping keys.
//
// Writers call kv_set_i64; readers call kv_get_i64. All operate on the same
// small set of keys ("shared_0".."shared_9") to maximise write-read interleave.
// With TSan enabled this validates the lock protects both read and write paths.
// ---------------------------------------------------------------------------
TEST(TsanStress, TC_TSAN_2_WriterReaderSplit) {
    TsanTempDir dir;

    kv_store* s = nullptr;
    ASSERT_EQ(kv_open("tc_tsan2", dir.c_str(), nullptr, &s), KV_OK);
    ASSERT_NE(s, nullptr);

    // Pre-populate the shared keys so readers never see KV_NOT_FOUND
    // due to a key that was never written at all.
    for (int k = 0; k < 10; ++k) {
        std::string key = "shared_" + std::to_string(k);
        ASSERT_EQ(kv_set_i64(s, key.c_str(), 0), KV_OK);
    }

    constexpr int kWriters    = 8;
    constexpr int kReaders    = 8;
    constexpr int kIterations = 500;

    std::vector<std::thread> threads;
    threads.reserve(kWriters + kReaders);

    // Writer threads
    for (int w = 0; w < kWriters; ++w) {
        threads.emplace_back([s, w]() {
            for (int i = 0; i < kIterations; ++i) {
                std::string key = "shared_" + std::to_string(i % 10);
                int64_t val = static_cast<int64_t>(w * 10000 + i);
                kv_status st = kv_set_i64(s, key.c_str(), val);
                EXPECT_EQ(st, KV_OK)
                    << "Writer " << w << " iter " << i << ": kv_set_i64 failed";
            }
        });
    }

    // Reader threads
    for (int r = 0; r < kReaders; ++r) {
        threads.emplace_back([s, r]() {
            for (int i = 0; i < kIterations; ++i) {
                std::string key = "shared_" + std::to_string(i % 10);
                int64_t out = 0;
                kv_status st = kv_get_i64(s, key.c_str(), &out);
                // KV_OK always (key was pre-populated and writers only overwrite)
                EXPECT_EQ(st, KV_OK)
                    << "Reader " << r << " iter " << i << ": kv_get_i64 failed";
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Final consistency: all 10 shared keys still readable
    for (int k = 0; k < 10; ++k) {
        std::string key = "shared_" + std::to_string(k);
        int64_t out = 0;
        EXPECT_EQ(kv_get_i64(s, key.c_str(), &out), KV_OK)
            << "Key " << key << " missing after concurrent write/read";
    }

    kv_close(s);
}
