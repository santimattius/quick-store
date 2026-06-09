#include "quickstore/writer.h"
#include "quickstore/reader.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// RAII temp directory helper
struct TempDir {
    std::string path;

    TempDir() {
        char tmpl[] = "/tmp/qs_test_XXXXXX";
        const char* d = ::mkdtemp(tmpl);
        EXPECT_NE(d, nullptr) << "mkdtemp failed";
        path = d ? d : "";
    }

    ~TempDir() {
        if (!path.empty()) {
            fs::remove_all(path);
        }
    }
};

// TC-WRT-1: open new file — bool true written and read back via Reader
TEST(WriterRoundtrip, TC_WRT_1_NewFileBoolTrue) {
    TempDir tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("test", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setBool("flag", true));
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("test", tmp.path);
    ASSERT_TRUE(r.has_value());
    auto v = r->getBool("flag");
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(*v);
}

// TC-WRT-2: set all 8 types, close, reopen with Reader, verify all
TEST(WriterRoundtrip, TC_WRT_2_AllTypesRoundtrip) {
    TempDir tmp;
    const uint8_t bytes[] = {0xDE, 0xAD, 0xBE, 0xEF};
    {
        auto w = quickstore::QuickStoreWriter::open("all_types", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setBool("b", false));
        EXPECT_TRUE(w->setInt32("i32", -42));
        EXPECT_TRUE(w->setInt64("i64", -9000000000LL));
        EXPECT_TRUE(w->setUInt64("u64", 18446744073709551615ULL));
        EXPECT_TRUE(w->setFloat("f", 3.14f));
        EXPECT_TRUE(w->setDouble("d", 2.718281828459045));
        EXPECT_TRUE(w->setString("s", "hello world"));
        EXPECT_TRUE(w->setBytes("by", bytes, sizeof(bytes)));
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("all_types", tmp.path);
    ASSERT_TRUE(r.has_value());

    auto b = r->getBool("b");
    ASSERT_TRUE(b.has_value()); EXPECT_FALSE(*b);

    auto i32 = r->getInt32("i32");
    ASSERT_TRUE(i32.has_value()); EXPECT_EQ(*i32, -42);

    auto i64 = r->getInt64("i64");
    ASSERT_TRUE(i64.has_value()); EXPECT_EQ(*i64, -9000000000LL);

    auto u64 = r->getUInt64("u64");
    ASSERT_TRUE(u64.has_value()); EXPECT_EQ(*u64, 18446744073709551615ULL);

    auto d = r->getDouble("d");
    ASSERT_TRUE(d.has_value()); EXPECT_DOUBLE_EQ(*d, 2.718281828459045);

    auto s = r->getString("s");
    ASSERT_TRUE(s.has_value()); EXPECT_EQ(*s, "hello world");

    auto by = r->getBytes("by");
    ASSERT_TRUE(by.has_value());
    ASSERT_EQ(by->size(), sizeof(bytes));
    EXPECT_EQ(std::memcmp(by->data(), bytes, sizeof(bytes)), 0);
}

// TC-WRT-3: setInt32 twice same key → last value wins
TEST(WriterRoundtrip, TC_WRT_3_OverwriteSameKey) {
    TempDir tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("overwrite", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("k", 100));
        EXPECT_TRUE(w->setInt32("k", 999));
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("overwrite", tmp.path);
    ASSERT_TRUE(r.has_value());
    auto v = r->getInt32("k");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 999);
}

// TC-WRT-4: setInt32 then remove → Reader: contains == false
TEST(WriterRoundtrip, TC_WRT_4_RemoveKey) {
    TempDir tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("remove_test", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("gone", 42));
        EXPECT_TRUE(w->remove("gone"));
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("remove_test", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->contains("gone"));
    EXPECT_EQ(r->count(), 0u);
}

// TC-WRT-5: count/allKeys/contains correct after mixed ops
TEST(WriterRoundtrip, TC_WRT_5_CountContainsAllKeys) {
    TempDir tmp;
    auto w = quickstore::QuickStoreWriter::open("mixed", tmp.path);
    ASSERT_TRUE(w.has_value());
    EXPECT_TRUE(w->setInt32("a", 1));
    EXPECT_TRUE(w->setInt32("b", 2));
    EXPECT_TRUE(w->setInt32("c", 3));
    EXPECT_TRUE(w->remove("b"));

    EXPECT_EQ(w->count(), 2u);
    EXPECT_TRUE(w->contains("a"));
    EXPECT_FALSE(w->contains("b"));
    EXPECT_TRUE(w->contains("c"));

    auto keys = w->allKeys();
    EXPECT_EQ(keys.size(), 2u);
    bool hasA = false, hasC = false;
    for (const auto& k : keys) {
        if (k == "a") hasA = true;
        if (k == "c") hasC = true;
    }
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasC);
}

// TC-WRT-6: close() idempotent — second close is no-op
TEST(WriterRoundtrip, TC_WRT_6_CloseIdempotent) {
    TempDir tmp;
    auto w = quickstore::QuickStoreWriter::open("idem", tmp.path);
    ASSERT_TRUE(w.has_value());
    w->close();
    EXPECT_NO_FATAL_FAILURE(w->close()); // second close must not crash
}

// TC-WRT-8: clearAll() removes all keys; Reader opens empty store after clearAll
TEST(WriterRoundtrip, TC_WRT_8_ClearAll) {
    TempDir tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("clearall", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("a", 1));
        EXPECT_TRUE(w->setString("b", "hello"));
        EXPECT_EQ(w->count(), 2u);

        EXPECT_TRUE(w->clearAll());

        // Immediately after clearAll, count must be 0
        EXPECT_EQ(w->count(), 0u);
        EXPECT_FALSE(w->contains("a"));
        EXPECT_FALSE(w->contains("b"));
        w->close();
    }
    // Reader must also see an empty store
    auto r = quickstore::QuickStoreReader::open("clearall", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->count(), 0u);
    EXPECT_FALSE(r->contains("a"));
    EXPECT_FALSE(r->contains("b"));
}

// TC-WRT-7: Writer getX read-through before close
TEST(WriterRoundtrip, TC_WRT_7_ReadThroughBeforeClose) {
    TempDir tmp;
    auto w = quickstore::QuickStoreWriter::open("readthrough", tmp.path);
    ASSERT_TRUE(w.has_value());
    EXPECT_TRUE(w->setInt32("x", 77));
    EXPECT_TRUE(w->setString("s", "abc"));

    auto xi = w->getInt32("x");
    ASSERT_TRUE(xi.has_value()); EXPECT_EQ(*xi, 77);

    auto xs = w->getString("s");
    ASSERT_TRUE(xs.has_value()); EXPECT_EQ(*xs, "abc");

    EXPECT_TRUE(w->contains("x"));
    EXPECT_EQ(w->count(), 2u);
}
