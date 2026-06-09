#include "quickstore/codec.h"
#include "quickstore/varint.h"
#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>

using namespace quickstore;

static std::vector<uint8_t> encodeUInt64(uint64_t v) {
    std::vector<uint8_t> buf;
    while (v & ~uint64_t(0x7F)) {
        buf.push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    buf.push_back(static_cast<uint8_t>(v & 0x7F));
    return buf;
}

template<typename T>
static std::vector<uint8_t> asLE(T val) {
    std::vector<uint8_t> buf(sizeof(T));
    std::memcpy(buf.data(), &val, sizeof(T));
    return buf;
}

// TC-COD-1
TEST(Codec, DecodeBool_False) {
    uint8_t b = 0x00;
    auto v = decodeBool(&b, 1);
    ASSERT_TRUE(v.has_value());
    EXPECT_FALSE(*v);
}

// TC-COD-2
TEST(Codec, DecodeBool_True) {
    uint8_t b = 0x01;
    auto v = decodeBool(&b, 1);
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(*v);
}

// TC-COD-3
TEST(Codec, DecodeBool_InvalidByte) {
    uint8_t b = 0x02;
    auto v = decodeBool(&b, 1);
    EXPECT_FALSE(v.has_value());
}

// TC-COD-4
TEST(Codec, DecodeInt32_Negative1) {
    auto buf = encodeUInt64(static_cast<uint64_t>(int64_t(-1)));
    auto v = decodeInt32(buf.data(), buf.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, -1);
}

// TC-COD-5
TEST(Codec, DecodeInt32_Min) {
    int32_t val = std::numeric_limits<int32_t>::min();
    auto buf = encodeUInt64(static_cast<uint64_t>(int64_t(val)));
    auto v = decodeInt32(buf.data(), buf.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, val);
}

// TC-COD-6
TEST(Codec, DecodeUInt32_Boundary) {
    for (uint32_t val : {uint32_t(0), uint32_t(1), std::numeric_limits<uint32_t>::max()}) {
        auto buf = encodeUInt64(val);
        auto v = decodeUInt32(buf.data(), buf.size());
        ASSERT_TRUE(v.has_value()) << "val=" << val;
        EXPECT_EQ(*v, val) << "val=" << val;
    }
}

// TC-COD-7
TEST(Codec, DecodeInt64_Negative) {
    for (int64_t val : {int64_t(-1), std::numeric_limits<int64_t>::min()}) {
        auto buf = encodeUInt64(static_cast<uint64_t>(val));
        auto v = decodeInt64(buf.data(), buf.size());
        ASSERT_TRUE(v.has_value()) << "val=" << val;
        EXPECT_EQ(*v, val) << "val=" << val;
    }
}

// TC-COD-8
TEST(Codec, DecodeUInt64_Boundary) {
    for (uint64_t val : {uint64_t(0), std::numeric_limits<uint64_t>::max()}) {
        auto buf = encodeUInt64(val);
        auto v = decodeUInt64(buf.data(), buf.size());
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, val);
    }
}

// TC-COD-9
TEST(Codec, DecodeFloat_Zero) {
    float f = 0.0f;
    auto buf = asLE(f);
    auto v = decodeFloat(buf.data(), buf.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 0.0f);
}

// TC-COD-10
TEST(Codec, DecodeFloat_NaN) {
    float f = std::numeric_limits<float>::quiet_NaN();
    auto buf = asLE(f);
    auto v = decodeFloat(buf.data(), buf.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(std::isnan(*v));
}

// TC-COD-11
TEST(Codec, DecodeFloat_Infinity) {
    float pos_inf = std::numeric_limits<float>::infinity();
    auto buf1 = asLE(pos_inf);
    auto v1 = decodeFloat(buf1.data(), buf1.size());
    ASSERT_TRUE(v1.has_value());
    EXPECT_TRUE(std::isinf(*v1) && *v1 > 0);

    float neg_inf = -std::numeric_limits<float>::infinity();
    auto buf2 = asLE(neg_inf);
    auto v2 = decodeFloat(buf2.data(), buf2.size());
    ASSERT_TRUE(v2.has_value());
    EXPECT_TRUE(std::isinf(*v2) && *v2 < 0);
}

// TC-COD-12: -0.0 round-trip (signbit must be preserved)
TEST(Codec, DecodeDouble_NegativeZero) {
    double d = -0.0;
    auto buf = asLE(d);
    auto v = decodeDouble(buf.data(), buf.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(std::signbit(*v));
}

// TC-COD-13
TEST(Codec, DecodeDouble_NaN) {
    double d = std::numeric_limits<double>::quiet_NaN();
    auto buf = asLE(d);
    auto v = decodeDouble(buf.data(), buf.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_TRUE(std::isnan(*v));
}

// TC-COD-14
TEST(Codec, DecodeString_ASCII) {
    std::string s = "hello";
    auto v = decodeString(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "hello");
}

// TC-COD-15: UTF-8 CJK (世界 = 3-byte sequences)
TEST(Codec, DecodeString_CJK) {
    std::string s = "\xe4\xb8\x96\xe7\x95\x8c"; // 世界
    auto v = decodeString(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, s);
}

// TC-COD-16: emoji 🌍 (4-byte UTF-8)
TEST(Codec, DecodeString_Emoji) {
    std::string s = "\xf0\x9f\x8c\x8d"; // 🌍
    auto v = decodeString(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, s);
}

// TC-COD-17: bytes round-trip
TEST(Codec, DecodeBytes_RoundTrip) {
    std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF};
    auto v = decodeBytes(data.data(), data.size());
    ASSERT_TRUE(v.has_value());
    ASSERT_EQ(v->size(), data.size());
    EXPECT_EQ(std::vector<uint8_t>(v->data(), v->data() + v->size()), data);
}
