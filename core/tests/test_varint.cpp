#include "quickstore/varint.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>
#include <vector>

using namespace quickstore::detail;

static std::vector<uint8_t> encodeUInt64(uint64_t v) {
    std::vector<uint8_t> buf;
    while (v & ~uint64_t(0x7F)) {
        buf.push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    buf.push_back(static_cast<uint8_t>(v & 0x7F));
    return buf;
}

static std::vector<uint8_t> encodeInt32(int32_t v) {
    return encodeUInt64(static_cast<uint64_t>(static_cast<int64_t>(v)));
}

static std::vector<uint8_t> encodeInt64(int64_t v) {
    return encodeUInt64(static_cast<uint64_t>(v));
}

// TC-VAR-1: single byte 0x00 → uint32=0, consumed=1
TEST(Varint, UInt32_Zero) {
    std::vector<uint8_t> buf = {0x00};
    uint32_t out = 99; bool err = false;
    size_t n = readUInt32(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 1u);
    EXPECT_EQ(out, 0u);
}

// TC-VAR-2: single byte 0x7F → uint32=127, consumed=1
TEST(Varint, UInt32_127) {
    std::vector<uint8_t> buf = {0x7F};
    uint32_t out = 0; bool err = false;
    size_t n = readUInt32(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 1u);
    EXPECT_EQ(out, 127u);
}

// TC-VAR-3: two bytes {0x80, 0x01} → uint32=128, consumed=2
TEST(Varint, UInt32_128) {
    std::vector<uint8_t> buf = {0x80, 0x01};
    uint32_t out = 0; bool err = false;
    size_t n = readUInt32(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 2u);
    EXPECT_EQ(out, 128u);
}

// TC-VAR-4: uint32 max (5 bytes)
TEST(Varint, UInt32_Max) {
    auto buf = encodeUInt64(std::numeric_limits<uint32_t>::max());
    ASSERT_EQ(buf.size(), 5u);
    uint32_t out = 0; bool err = false;
    size_t n = readUInt32(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 5u);
    EXPECT_EQ(out, std::numeric_limits<uint32_t>::max());
}

// TC-VAR-5: uint64 zero
TEST(Varint, UInt64_Zero) {
    std::vector<uint8_t> buf = {0x00};
    uint64_t out = 99; bool err = false;
    size_t n = readUInt64(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 1u);
    EXPECT_EQ(out, 0u);
}

// TC-VAR-6: uint64 max (10 bytes)
TEST(Varint, UInt64_Max) {
    auto buf = encodeUInt64(std::numeric_limits<uint64_t>::max());
    ASSERT_EQ(buf.size(), 10u);
    uint64_t out = 0; bool err = false;
    size_t n = readUInt64(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 10u);
    EXPECT_EQ(out, std::numeric_limits<uint64_t>::max());
}

// TC-VAR-7: int32(-1) → 10 bytes (promoted to uint64 before encoding)
TEST(Varint, Int32_Negative1_Consumes10Bytes) {
    auto buf = encodeInt32(-1);
    ASSERT_EQ(buf.size(), 10u) << "int32(-1) must encode as 10 bytes";
    int32_t out = 0; bool err = false;
    size_t n = readInt32(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 10u);
    EXPECT_EQ(out, -1);
}

// TC-VAR-8: int32(INT32_MIN) → 10 bytes
TEST(Varint, Int32_Min) {
    auto buf = encodeInt32(std::numeric_limits<int32_t>::min());
    ASSERT_EQ(buf.size(), 10u);
    int32_t out = 0; bool err = false;
    size_t n = readInt32(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 10u);
    EXPECT_EQ(out, std::numeric_limits<int32_t>::min());
}

// TC-VAR-9: int64(INT64_MIN) → 10 bytes
TEST(Varint, Int64_Min) {
    auto buf = encodeInt64(std::numeric_limits<int64_t>::min());
    ASSERT_EQ(buf.size(), 10u);
    int64_t out = 0; bool err = false;
    size_t n = readInt64(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 10u);
    EXPECT_EQ(out, std::numeric_limits<int64_t>::min());
}

// TC-VAR-10: int64(-1) → 10 bytes
TEST(Varint, Int64_Negative1) {
    auto buf = encodeInt64(-1);
    ASSERT_EQ(buf.size(), 10u);
    int64_t out = 0; bool err = false;
    size_t n = readInt64(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_FALSE(err);
    EXPECT_EQ(n, 10u);
    EXPECT_EQ(out, int64_t(-1));
}

// TC-VAR-11: error on empty buffer
TEST(Varint, ErrorOnEmptyBuffer) {
    uint8_t dummy = 0;
    uint32_t out = 0; bool err = false;
    readUInt32(&dummy, &dummy, out, err);
    EXPECT_TRUE(err);
}

// TC-VAR-12: error on truncated input (need 2 bytes, only 1 available)
TEST(Varint, ErrorOnTruncatedInput) {
    std::vector<uint8_t> buf = {0x80}; // continuation bit set, but no next byte
    uint32_t out = 0; bool err = false;
    readUInt32(buf.data(), buf.data() + buf.size(), out, err);
    EXPECT_TRUE(err);
}
