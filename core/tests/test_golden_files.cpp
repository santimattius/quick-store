#include "quickstore/reader.h"
#include <filesystem>
#include <functional>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <limits>

namespace fs = std::filesystem;

static fs::path goldenRoot() {
    return fs::path(QUICKSTORE_GOLDEN_DIR);
}

struct GoldenScenario {
    std::string name;
    std::function<void(quickstore::QuickStoreReader&)> validate;
};

class GoldenTest : public ::testing::TestWithParam<GoldenScenario> {};

TEST_P(GoldenTest, ReadMatchesMMKV) {
    const auto& scenario = GetParam();
    fs::path dir = goldenRoot() / scenario.name;
    if (!fs::exists(dir / "mmkv") || !fs::exists(dir / "mmkv.crc")) {
        GTEST_SKIP() << "Golden files not found for '" << scenario.name
                     << "'. Run core/tests/golden/_generator/generate_golden <golden_dir>.";
    }
    auto reader = quickstore::QuickStoreReader::open("mmkv", dir.string());
    ASSERT_TRUE(reader.has_value()) << "Failed to open golden: " << dir;
    scenario.validate(*reader);
}

INSTANTIATE_TEST_SUITE_P(
    GoldenSuite, GoldenTest,
    ::testing::Values(
        GoldenScenario{"single_bool", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 1u);
            EXPECT_THAT(r.allKeys(), ::testing::UnorderedElementsAre("b"));
            auto v = r.getBool("b");
            ASSERT_TRUE(v.has_value());
            EXPECT_TRUE(*v);
        }},
        GoldenScenario{"negative_int32", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 1u);
            auto v = r.getInt32("i");
            ASSERT_TRUE(v.has_value());
            EXPECT_EQ(*v, -1);
        }},
        GoldenScenario{"negative_int64", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 1u);
            auto v = r.getInt64("l");
            ASSERT_TRUE(v.has_value());
            EXPECT_EQ(*v, std::numeric_limits<int64_t>::min());
        }},
        GoldenScenario{"float_double", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 2u);
            auto dv = r.getDouble("d");
            ASSERT_TRUE(dv.has_value());
            EXPECT_TRUE(std::isinf(*dv) && *dv < 0.0);
            // "f" is a float NaN stored as 4 bytes; read as raw bytes and verify NaN bits
            auto fv = r.getBytes("f");
            ASSERT_TRUE(fv.has_value());
            ASSERT_EQ(fv->size(), 4u);
            float f_val; std::memcpy(&f_val, fv->data(), 4);
            EXPECT_TRUE(std::isnan(f_val));
        }},
        GoldenScenario{"string_unicode", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 1u);
            auto v = r.getString("s");
            ASSERT_TRUE(v.has_value());
            std::string expected = "hello \xe4\xb8\x96\xe7\x95\x8c \xf0\x9f\x8c\x8d";
            EXPECT_EQ(*v, expected);
        }},
        GoldenScenario{"bytes_large", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 1u);
            auto v = r.getBytes("raw");
            ASSERT_TRUE(v.has_value());
            ASSERT_EQ(v->size(), 4096u);
            for (size_t i = 0; i < v->size(); ++i)
                EXPECT_EQ(v->data()[i], 0u) << "byte " << i << " not zero";
        }},
        GoldenScenario{"overwrite_same_key", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 1u);
            auto v = r.getInt32("k");
            ASSERT_TRUE(v.has_value());
            EXPECT_EQ(*v, 99);
        }},
        GoldenScenario{"tombstone", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 0u);
            EXPECT_FALSE(r.contains("k"));
            EXPECT_FALSE(r.getInt32("k").has_value());
        }},
        GoldenScenario{"all_types", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 9u);
            EXPECT_FALSE(*r.getBool("bool_f"));
            EXPECT_TRUE(*r.getBool("bool_t"));
            EXPECT_EQ(*r.getInt32("i32"), int32_t(-42));
            EXPECT_EQ(*r.getInt64("i64"), int64_t(-1));
            EXPECT_EQ(*r.getUInt64("u64"), std::numeric_limits<uint64_t>::max());
            auto dbl = r.getDouble("dbl");
            ASSERT_TRUE(dbl.has_value());
            EXPECT_NEAR(*dbl, 2.718281828, 1e-9);
            EXPECT_EQ(*r.getString("str"), "utf8");
            auto raw = r.getBytes("raw");
            ASSERT_TRUE(raw.has_value());
            EXPECT_EQ(raw->size(), 4u);
        }},
        GoldenScenario{"many_keys", [](quickstore::QuickStoreReader& r) {
            EXPECT_EQ(r.count(), 1000u);
            for (int i = 0; i < 1000; ++i) {
                std::string key = "key_" + std::to_string(i);
                std::string expected = "val_" + std::to_string(i);
                EXPECT_TRUE(r.contains(key)) << "missing: " << key;
                auto v = r.getString(key);
                ASSERT_TRUE(v.has_value()) << "nullopt for: " << key;
                EXPECT_EQ(*v, expected);
            }
        }},
        GoldenScenario{"recovery_corrupt", [](quickstore::QuickStoreReader& r) {
            // lastConfirmed path: a=1 and b=2 recovered; c (corrupted) is absent
            EXPECT_EQ(r.count(), 2u);
            EXPECT_TRUE(r.contains("a"));
            EXPECT_TRUE(r.contains("b"));
            EXPECT_FALSE(r.contains("c"));
            EXPECT_EQ(*r.getInt32("a"), 1);
            EXPECT_EQ(*r.getInt32("b"), 2);
        }}
    ),
    [](const ::testing::TestParamInfo<GoldenScenario>& info) {
        return info.param.name;
    }
);
