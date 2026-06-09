#include "quickstore/kv_store.h"
#include "quickstore/writer.h"
#include "quickstore/reader.h"
#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// RAII temp directory helper
// ============================================================

struct TempDirCABI {
    std::string path;

    TempDirCABI() {
        char tmpl[] = "/tmp/qs_cabi_XXXXXX";
        const char* d = ::mkdtemp(tmpl);
        EXPECT_NE(d, nullptr) << "mkdtemp failed";
        path = d ? d : "";
    }

    ~TempDirCABI() {
        if (!path.empty()) {
            fs::remove_all(path);
        }
    }
};

// ============================================================
// GTest fixture: opens a store in SetUp, closes in TearDown
// ============================================================

class KVStoreCABITest : public ::testing::Test {
protected:
    TempDirCABI tmp;
    kv_store*   store = nullptr;

    void SetUp() override {
        kv_status st = kv_open("test", tmp.path.c_str(), nullptr, &store);
        ASSERT_EQ(st, KV_OK);
        ASSERT_NE(store, nullptr);
    }

    void TearDown() override {
        if (store) {
            kv_close(store);
            store = nullptr;
        }
    }
};

// ============================================================
// TC-CABI-OPEN — Lifecycle
// ============================================================

// TC-CABI-OPEN-1: NULL mmkv_id -> KV_INVALID_ARG
TEST(KVStoreCABIOpen, TC_CABI_OPEN_1_NullId) {
    TempDirCABI tmp;
    kv_store* s = nullptr;
    EXPECT_EQ(kv_open(nullptr, tmp.path.c_str(), nullptr, &s), KV_INVALID_ARG);
    EXPECT_EQ(s, nullptr);
}

// TC-CABI-OPEN-2: NULL root_dir -> KV_INVALID_ARG
TEST(KVStoreCABIOpen, TC_CABI_OPEN_2_NullDir) {
    kv_store* s = nullptr;
    EXPECT_EQ(kv_open("test", nullptr, nullptr, &s), KV_INVALID_ARG);
    EXPECT_EQ(s, nullptr);
}

// TC-CABI-OPEN-3: valid id + dir, opts NULL -> KV_OK, store != NULL
TEST(KVStoreCABIOpen, TC_CABI_OPEN_3_ValidOpen) {
    TempDirCABI tmp;
    kv_store* s = nullptr;
    EXPECT_EQ(kv_open("test", tmp.path.c_str(), nullptr, &s), KV_OK);
    EXPECT_NE(s, nullptr);
    kv_close(s);
}

// TC-CABI-OPEN-4: open/close/reopen -> KV_OK, data persists
TEST(KVStoreCABIOpen, TC_CABI_OPEN_4_CloseReopen) {
    TempDirCABI tmp;
    {
        kv_store* s = nullptr;
        ASSERT_EQ(kv_open("test", tmp.path.c_str(), nullptr, &s), KV_OK);
        ASSERT_EQ(kv_set_i64(s, "n", 42), KV_OK);
        kv_close(s);
    }
    {
        kv_store* s = nullptr;
        ASSERT_EQ(kv_open("test", tmp.path.c_str(), nullptr, &s), KV_OK);
        int64_t v = 0;
        ASSERT_EQ(kv_get_i64(s, "n", &v), KV_OK);
        EXPECT_EQ(v, 42);
        kv_close(s);
    }
}

// TC-CABI-OPEN-5: QuickStoreWriter produces file; kv_open reads it cleanly
TEST(KVStoreCABIOpen, TC_CABI_OPEN_5_InteropWithWriter) {
    TempDirCABI tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("compat", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setBool("flag", true));
        EXPECT_TRUE(w->setInt64("num", -999));
        w->close();
    }
    kv_store* s = nullptr;
    ASSERT_EQ(kv_open("compat", tmp.path.c_str(), nullptr, &s), KV_OK);
    int flag = -1;
    int64_t num = 0;
    ASSERT_EQ(kv_get_bool(s, "flag", &flag), KV_OK);
    EXPECT_EQ(flag, 1);
    ASSERT_EQ(kv_get_i64(s, "num", &num), KV_OK);
    EXPECT_EQ(num, -999);
    kv_close(s);
}

// ============================================================
// TC-CABI-SET — Mutations
// ============================================================

// TC-CABI-SET-1: kv_set_bool + kv_get_bool round-trip true and false
TEST_F(KVStoreCABITest, TC_CABI_SET_1_BoolRoundTrip) {
    ASSERT_EQ(kv_set_bool(store, "t", 1), KV_OK);
    ASSERT_EQ(kv_set_bool(store, "f", 0), KV_OK);
    int t = -1, f = -1;
    ASSERT_EQ(kv_get_bool(store, "t", &t), KV_OK);
    ASSERT_EQ(kv_get_bool(store, "f", &f), KV_OK);
    EXPECT_EQ(t, 1);
    EXPECT_EQ(f, 0);
}

// TC-CABI-SET-2: kv_set_i64 + kv_get_i64 round-trip INT64_MIN, INT64_MAX, 0
TEST_F(KVStoreCABITest, TC_CABI_SET_2_Int64RoundTrip) {
    ASSERT_EQ(kv_set_i64(store, "min", INT64_MIN), KV_OK);
    ASSERT_EQ(kv_set_i64(store, "max", INT64_MAX), KV_OK);
    ASSERT_EQ(kv_set_i64(store, "zero", 0), KV_OK);
    int64_t vmin = 1, vmax = 1, vzero = 1;
    ASSERT_EQ(kv_get_i64(store, "min", &vmin), KV_OK);
    ASSERT_EQ(kv_get_i64(store, "max", &vmax), KV_OK);
    ASSERT_EQ(kv_get_i64(store, "zero", &vzero), KV_OK);
    EXPECT_EQ(vmin, INT64_MIN);
    EXPECT_EQ(vmax, INT64_MAX);
    EXPECT_EQ(vzero, 0);
}

// TC-CABI-SET-3: kv_set_double + kv_get_double round-trip NaN, +Inf, -Inf, -0.0
TEST_F(KVStoreCABITest, TC_CABI_SET_3_DoubleRoundTrip) {
    double nan_val  = std::numeric_limits<double>::quiet_NaN();
    double pinf     = std::numeric_limits<double>::infinity();
    double ninf     = -std::numeric_limits<double>::infinity();
    double neg_zero = -0.0;
    ASSERT_EQ(kv_set_double(store, "nan",   nan_val),  KV_OK);
    ASSERT_EQ(kv_set_double(store, "pinf",  pinf),     KV_OK);
    ASSERT_EQ(kv_set_double(store, "ninf",  ninf),     KV_OK);
    ASSERT_EQ(kv_set_double(store, "nzero", neg_zero), KV_OK);
    double vnan = 0, vpinf = 0, vninf = 0, vnzero = 0;
    ASSERT_EQ(kv_get_double(store, "nan",   &vnan),   KV_OK);
    ASSERT_EQ(kv_get_double(store, "pinf",  &vpinf),  KV_OK);
    ASSERT_EQ(kv_get_double(store, "ninf",  &vninf),  KV_OK);
    ASSERT_EQ(kv_get_double(store, "nzero", &vnzero), KV_OK);
    EXPECT_TRUE(std::isnan(vnan));
    EXPECT_TRUE(std::isinf(vpinf) && vpinf > 0);
    EXPECT_TRUE(std::isinf(vninf) && vninf < 0);
    // -0.0 and 0.0 compare equal; verify sign bit
    EXPECT_EQ(std::signbit(vnzero), true);
}

// TC-CABI-SET-4: kv_set_bytes + kv_get_bytes round-trip 4096-byte buffer
TEST_F(KVStoreCABITest, TC_CABI_SET_4_BytesRoundTrip) {
    std::vector<uint8_t> data(4096);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i & 0xFF);
    ASSERT_EQ(kv_set_bytes(store, "blob", data.data(), data.size()), KV_OK);
    kv_buffer buf{};
    ASSERT_EQ(kv_get_bytes(store, "blob", &buf), KV_OK);
    ASSERT_EQ(buf.len, data.size());
    EXPECT_EQ(std::memcmp(buf.data, data.data(), data.size()), 0);
    kv_buffer_free(&buf);
}

// TC-CABI-SET-5: kv_set_bytes(key, NULL, 0) -> KV_OK (empty bytes valid)
TEST_F(KVStoreCABITest, TC_CABI_SET_5_EmptyBytesOk) {
    EXPECT_EQ(kv_set_bytes(store, "empty", nullptr, 0), KV_OK);
}

// TC-CABI-SET-6: kv_set_bytes(key, NULL, 1) -> KV_INVALID_ARG
TEST_F(KVStoreCABITest, TC_CABI_SET_6_NullDataNonzeroLen) {
    EXPECT_EQ(kv_set_bytes(store, "bad", nullptr, 1), KV_INVALID_ARG);
}

// ============================================================
// TC-CABI-GET — Queries
// ============================================================

// TC-CABI-GET-1: kv_get_bool absent key -> KV_NOT_FOUND
TEST_F(KVStoreCABITest, TC_CABI_GET_1_AbsentKey) {
    int v = -1;
    EXPECT_EQ(kv_get_bool(store, "no_such_key", &v), KV_NOT_FOUND);
    EXPECT_EQ(v, -1); // unchanged
}

// TC-CABI-GET-2: kv_get_bytes -> heap-allocated; kv_buffer_free frees it without crash
TEST_F(KVStoreCABITest, TC_CABI_GET_2_BytesFreeNoCrash) {
    uint8_t data[] = {1, 2, 3};
    ASSERT_EQ(kv_set_bytes(store, "k", data, 3), KV_OK);
    kv_buffer buf{};
    ASSERT_EQ(kv_get_bytes(store, "k", &buf), KV_OK);
    ASSERT_NE(buf.data, nullptr);
    EXPECT_EQ(buf.len, 3u);
    kv_buffer_free(&buf);
    EXPECT_EQ(buf.data, nullptr); // zeroed after free
    EXPECT_EQ(buf.len, 0u);
}

// TC-CABI-GET-3: kv_get_into sufficient buffer -> KV_OK, bytes correct
TEST_F(KVStoreCABITest, TC_CABI_GET_3_GetIntoSufficientBuffer) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    ASSERT_EQ(kv_set_bytes(store, "k", data, 3), KV_OK);
    uint8_t buf[8] = {};
    size_t written = 0;
    ASSERT_EQ(kv_get_into(store, "k", buf, 8, &written), KV_OK);
    EXPECT_EQ(written, 3u);
    EXPECT_EQ(std::memcmp(buf, data, 3), 0);
}

// TC-CABI-GET-4: kv_get_into undersized buffer -> KV_BUFFER_TOO_SMALL, no bytes written
TEST_F(KVStoreCABITest, TC_CABI_GET_4_GetIntoUndersized) {
    uint8_t data[] = {1, 2, 3, 4, 5};
    ASSERT_EQ(kv_set_bytes(store, "k", data, 5), KV_OK);
    uint8_t buf[2] = {0xFF, 0xFF};
    size_t written = 99;
    ASSERT_EQ(kv_get_into(store, "k", buf, 2, &written), KV_BUFFER_TOO_SMALL);
    EXPECT_EQ(written, 99u); // unchanged
    EXPECT_EQ(buf[0], 0xFF); // no partial write
}

// TC-CABI-GET-5: kv_get_bool(store, key, NULL) -> KV_INVALID_ARG
TEST_F(KVStoreCABITest, TC_CABI_GET_5_NullOutParam) {
    EXPECT_EQ(kv_get_bool(store, "x", nullptr), KV_INVALID_ARG);
}

// ============================================================
// TC-CABI-OPS — Store operations
// ============================================================

// TC-CABI-OPS-1: kv_contains correct true/false
TEST_F(KVStoreCABITest, TC_CABI_OPS_1_Contains) {
    ASSERT_EQ(kv_set_bool(store, "present", 1), KV_OK);
    int found = -1;
    ASSERT_EQ(kv_contains(store, "present", &found), KV_OK);
    EXPECT_EQ(found, 1);
    found = -1;
    ASSERT_EQ(kv_contains(store, "absent", &found), KV_OK);
    EXPECT_EQ(found, 0);
}

// TC-CABI-OPS-2: kv_remove -> key absent; count decremented
TEST_F(KVStoreCABITest, TC_CABI_OPS_2_Remove) {
    ASSERT_EQ(kv_set_bool(store, "a", 1), KV_OK);
    ASSERT_EQ(kv_set_bool(store, "b", 0), KV_OK);
    size_t cnt = 0;
    ASSERT_EQ(kv_count(store, &cnt), KV_OK);
    EXPECT_EQ(cnt, 2u);

    ASSERT_EQ(kv_remove(store, "a"), KV_OK);

    ASSERT_EQ(kv_count(store, &cnt), KV_OK);
    EXPECT_EQ(cnt, 1u);

    int found = -1;
    ASSERT_EQ(kv_contains(store, "a", &found), KV_OK);
    EXPECT_EQ(found, 0);
}

// TC-CABI-OPS-3: kv_count correct after mixed set/remove
TEST_F(KVStoreCABITest, TC_CABI_OPS_3_CountMixed) {
    ASSERT_EQ(kv_set_i64(store, "x", 1), KV_OK);
    ASSERT_EQ(kv_set_i64(store, "y", 2), KV_OK);
    ASSERT_EQ(kv_set_i64(store, "z", 3), KV_OK);
    ASSERT_EQ(kv_remove(store, "y"), KV_OK);
    size_t cnt = 0;
    ASSERT_EQ(kv_count(store, &cnt), KV_OK);
    EXPECT_EQ(cnt, 2u);
}

// TC-CABI-OPS-4: kv_all_keys empty store -> count==0, out_keys.data==NULL
TEST_F(KVStoreCABITest, TC_CABI_OPS_4_AllKeysEmpty) {
    kv_buffer keys{};
    size_t count = 99;
    ASSERT_EQ(kv_all_keys(store, &keys, &count), KV_OK);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(keys.data, nullptr);
    EXPECT_EQ(keys.len, 0u);
}

// TC-CABI-OPS-5: kv_all_keys 3 keys -> count==3, NUL-separated iteration correct
TEST_F(KVStoreCABITest, TC_CABI_OPS_5_AllKeysThree) {
    ASSERT_EQ(kv_set_bool(store, "alpha", 1), KV_OK);
    ASSERT_EQ(kv_set_bool(store, "beta", 0), KV_OK);
    ASSERT_EQ(kv_set_bool(store, "gamma", 1), KV_OK);

    kv_buffer keys{};
    size_t count = 0;
    ASSERT_EQ(kv_all_keys(store, &keys, &count), KV_OK);
    EXPECT_EQ(count, 3u);
    ASSERT_NE(keys.data, nullptr);

    // Collect the keys from the NUL-separated blob
    std::vector<std::string> got;
    const char* p = reinterpret_cast<const char*>(keys.data);
    for (size_t i = 0; i < count; ++i) {
        got.emplace_back(p);
        p += got.back().size() + 1;
    }
    EXPECT_EQ(got.size(), 3u);
    // Order is unordered_map order — just check all three are present
    EXPECT_NE(std::find(got.begin(), got.end(), "alpha"), got.end());
    EXPECT_NE(std::find(got.begin(), got.end(), "beta"), got.end());
    EXPECT_NE(std::find(got.begin(), got.end(), "gamma"), got.end());

    kv_buffer_free(&keys);
}

// TC-CABI-OPS-6: kv_trim -> KV_OK; QuickStoreReader opens result cleanly
TEST_F(KVStoreCABITest, TC_CABI_OPS_6_TrimCompacts) {
    // Write several keys and force fragmentation by removing some
    for (int i = 0; i < 10; ++i) {
        std::string k = "key" + std::to_string(i);
        ASSERT_EQ(kv_set_i64(store, k.c_str(), static_cast<int64_t>(i)), KV_OK);
    }
    for (int i = 0; i < 5; ++i) {
        std::string k = "key" + std::to_string(i);
        ASSERT_EQ(kv_remove(store, k.c_str()), KV_OK);
    }

    ASSERT_EQ(kv_trim(store), KV_OK);

    // Verify via QuickStoreReader that the file is still valid
    auto r = quickstore::QuickStoreReader::open("test", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->count(), 5u);
    r->close();
}

// TC-CABI-OPS-7: kv_clear -> count==0
TEST_F(KVStoreCABITest, TC_CABI_OPS_7_Clear) {
    ASSERT_EQ(kv_set_bool(store, "a", 1), KV_OK);
    ASSERT_EQ(kv_set_bool(store, "b", 0), KV_OK);
    ASSERT_EQ(kv_clear(store), KV_OK);
    size_t cnt = 99;
    ASSERT_EQ(kv_count(store, &cnt), KV_OK);
    EXPECT_EQ(cnt, 0u);
}

// ============================================================
// TC-CABI-THREAD — Thread safety
// ============================================================

// TC-CABI-THREAD-1: 4 threads x 25 unique keys -> count==100 after join
TEST(KVStoreCABIThread, TC_CABI_THREAD_1_ConcurrentWrites) {
    TempDirCABI tmp;
    kv_store* s = nullptr;
    ASSERT_EQ(kv_open("thr", tmp.path.c_str(), nullptr, &s), KV_OK);

    auto worker = [&](int thread_id) {
        for (int i = 0; i < 25; ++i) {
            std::string k = "t" + std::to_string(thread_id) + "_k" + std::to_string(i);
            kv_set_i64(s, k.c_str(), static_cast<int64_t>(i));
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) threads.emplace_back(worker, t);
    for (auto& th : threads) th.join();

    size_t cnt = 0;
    ASSERT_EQ(kv_count(s, &cnt), KV_OK);
    EXPECT_EQ(cnt, 100u);

    kv_close(s);
}

// TC-CABI-THREAD-2: concurrent get+set -> no crash or data corruption
TEST(KVStoreCABIThread, TC_CABI_THREAD_2_ConcurrentGetSet) {
    TempDirCABI tmp;
    kv_store* s = nullptr;
    ASSERT_EQ(kv_open("thr2", tmp.path.c_str(), nullptr, &s), KV_OK);
    ASSERT_EQ(kv_set_i64(s, "shared", 0), KV_OK);

    auto writer_fn = [&]() {
        for (int i = 0; i < 50; ++i) {
            kv_set_i64(s, "shared", static_cast<int64_t>(i));
        }
    };

    auto reader_fn = [&]() {
        int64_t v = 0;
        for (int i = 0; i < 50; ++i) {
            kv_get_i64(s, "shared", &v);
        }
    };

    std::thread w(writer_fn);
    std::thread r(reader_fn);
    w.join();
    r.join();

    // Just verify the store is still functional — no crash is the main criterion
    int64_t v = -1;
    ASSERT_EQ(kv_get_i64(s, "shared", &v), KV_OK);

    kv_close(s);
}

// ============================================================
// TC-CABI-REGRESSION — Full round-trip regression
// ============================================================

// TC-CABI-REGRESSION-1: QuickStoreWriter + QuickStoreReader full round-trip still works
// (value_blob.h extraction regression check)
TEST(KVStoreCABIRegression, TC_CABI_REGRESSION_1_WriterReaderRoundTrip) {
    TempDirCABI tmp;
    const uint8_t bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    {
        auto w = quickstore::QuickStoreWriter::open("reg", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setBool("b", true));
        EXPECT_TRUE(w->setInt64("i", -1234567890123LL));
        EXPECT_TRUE(w->setDouble("d", 3.14159));
        EXPECT_TRUE(w->setBytes("raw", bytes, sizeof(bytes)));
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("reg", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r->getBool("b"), true);
    EXPECT_EQ(*r->getInt64("i"), -1234567890123LL);
    EXPECT_NEAR(*r->getDouble("d"), 3.14159, 1e-10);
    auto raw = r->getBytes("raw");
    ASSERT_TRUE(raw.has_value());
    ASSERT_EQ(raw->size(), sizeof(bytes));
    EXPECT_EQ(std::memcmp(raw->data(), bytes, sizeof(bytes)), 0);
    r->close();
}

// ============================================================
// TC-VALBLOB — value_blob.h extraction — representative round-trips
// ============================================================

// TC-VALBLOB-1: QuickStoreReader still passes representative TC-READ-* scenario
// after value_blob.h extraction
TEST(KVStoreCABIValBlob, TC_VALBLOB_1_ReaderUnaffected) {
    TempDirCABI tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("vb_read", tmp.path);
        ASSERT_TRUE(w.has_value());
        w->setInt64("n", 42);
        w->setDouble("pi", 3.14);
        w->setString("s", "hello");
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("vb_read", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r->getInt64("n"), 42);
    EXPECT_NEAR(*r->getDouble("pi"), 3.14, 1e-12);
    EXPECT_EQ(*r->getString("s"), "hello");
    r->close();
}

// TC-VALBLOB-2: QuickStoreWriter still passes representative TC-WRT-* scenario
// after value_blob.h extraction
TEST(KVStoreCABIValBlob, TC_VALBLOB_2_WriterUnaffected) {
    TempDirCABI tmp;
    auto w = quickstore::QuickStoreWriter::open("vb_write", tmp.path);
    ASSERT_TRUE(w.has_value());
    EXPECT_TRUE(w->setBool("ok", true));
    EXPECT_TRUE(w->setInt64("big", INT64_MAX));
    EXPECT_TRUE(w->setString("str", "world"));
    // Verify via getter — exercises detail::getValueBlob on write path
    EXPECT_EQ(*w->getBool("ok"), true);
    EXPECT_EQ(*w->getInt64("big"), INT64_MAX);
    EXPECT_EQ(*w->getString("str"), "world");
    w->close();
}
