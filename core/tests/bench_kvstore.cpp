// bench_kvstore.cpp
// Google Benchmark targets for quickstore.
//
// Three benchmarks:
//   BM_SetI64_Sequential  — writes 10K distinct keys per iteration
//   BM_GetI64_Random      — reads random keys from a 10K pre-populated store
//   BM_SetI64_Overwrite   — writes the same key 10K times per iteration
//
// Build:
//   cmake -DQUICKSTORE_BUILD_BENCHMARKS=ON -B build_bench
//   cmake --build build_bench --target bench_kvstore
//
// Run:
//   ./build_bench/core/tests/bench_kvstore

#include "quickstore/kv_store.h"
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// TempDir RAII for benchmarks — mkdtemp, rm -rf on dtor.
// Each benchmark fixture creates its own TempDir so runs are isolated.
// ---------------------------------------------------------------------------
struct BenchTempDir {
    char path[64];
    BenchTempDir() {
        std::strncpy(path, "/tmp/qs_bench_XXXXXX", sizeof(path));
        if (::mkdtemp(path) == nullptr) {
            // Benchmark context — abort on setup failure
            ::abort();
        }
    }
    ~BenchTempDir() {
        std::string cmd = "rm -rf '";
        cmd += path;
        cmd += "'";
        ::system(cmd.c_str());
    }
    const char* c_str() const { return path; }
};

// ---------------------------------------------------------------------------
// BM_SetI64_Sequential
//
// Each benchmark iteration opens a fresh store and writes 10K distinct keys.
// Measures the sustained sequential write throughput of kv_set_i64.
// ---------------------------------------------------------------------------
static void BM_SetI64_Sequential(benchmark::State& state) {
    constexpr int kKeys = 10000;

    // Pre-build key strings outside the timed region
    std::vector<std::string> keys;
    keys.reserve(kKeys);
    for (int i = 0; i < kKeys; ++i) {
        keys.push_back("seq_key_" + std::to_string(i));
    }

    for (auto _ : state) {
        state.PauseTiming();
        BenchTempDir dir;
        kv_store* s = nullptr;
        if (kv_open("bm_seq", dir.c_str(), nullptr, &s) != KV_OK) {
            state.SkipWithError("kv_open failed in BM_SetI64_Sequential");
            return;
        }
        state.ResumeTiming();

        for (int i = 0; i < kKeys; ++i) {
            kv_set_i64(s, keys[i].c_str(), static_cast<int64_t>(i));
        }

        state.PauseTiming();
        kv_close(s);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * kKeys);
}
BENCHMARK(BM_SetI64_Sequential);

// ---------------------------------------------------------------------------
// BM_GetI64_Random
//
// Pre-populates a store with 10K keys once (in SetUp equivalent), then each
// iteration reads a random key. Measures random read throughput.
// ---------------------------------------------------------------------------
static void BM_GetI64_Random(benchmark::State& state) {
    constexpr int kKeys = 10000;

    BenchTempDir dir;
    std::vector<std::string> keys;
    keys.reserve(kKeys);
    for (int i = 0; i < kKeys; ++i) {
        keys.push_back("rnd_key_" + std::to_string(i));
    }

    // Populate once before timing loop
    kv_store* s = nullptr;
    if (kv_open("bm_rnd", dir.c_str(), nullptr, &s) != KV_OK) {
        state.SkipWithError("kv_open failed in BM_GetI64_Random setup");
        return;
    }
    for (int i = 0; i < kKeys; ++i) {
        kv_set_i64(s, keys[i].c_str(), static_cast<int64_t>(i));
    }

    int64_t out = 0;
    int idx = 0;
    for (auto _ : state) {
        // Cycle through keys deterministically (avoids rand() overhead)
        idx = (idx + 7919) % kKeys;  // prime step to approximate random access
        kv_get_i64(s, keys[idx].c_str(), &out);
        benchmark::DoNotOptimize(out);
    }

    kv_close(s);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GetI64_Random);

// ---------------------------------------------------------------------------
// BM_SetI64_Overwrite
//
// Writes the same single key 10K times per iteration. Measures overwrite
// throughput (append-log growth before compact).
// ---------------------------------------------------------------------------
static void BM_SetI64_Overwrite(benchmark::State& state) {
    constexpr int kWrites = 10000;
    const char* key = "overwrite_key";

    for (auto _ : state) {
        state.PauseTiming();
        BenchTempDir dir;
        kv_store* s = nullptr;
        if (kv_open("bm_ow", dir.c_str(), nullptr, &s) != KV_OK) {
            state.SkipWithError("kv_open failed in BM_SetI64_Overwrite");
            return;
        }
        state.ResumeTiming();

        for (int i = 0; i < kWrites; ++i) {
            kv_set_i64(s, key, static_cast<int64_t>(i));
        }

        state.PauseTiming();
        kv_close(s);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * kWrites);
}
BENCHMARK(BM_SetI64_Overwrite);

BENCHMARK_MAIN();
