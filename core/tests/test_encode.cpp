#include "quickstore/codec.h"
#include "quickstore/varint.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>
#include <cstring>

using namespace quickstore;

// Helper: encode into a fresh vector and return it.
template<typename Fn>
static std::vector<uint8_t> enc(Fn fn) {
    std::vector<uint8_t> out;
    fn(out);
    return out;
}

// TC-ENC-1: encodeBool false → {0x00}; true → {0x01}
TEST(EncodeTest, TC_ENC_1_Bool) {
    auto f = enc([](auto& o){ encodeBool(false, o); });
    ASSERT_EQ(f.size(), 1u);
    EXPECT_EQ(f[0], 0x00u);

    auto t = enc([](auto& o){ encodeBool(true, o); });
    ASSERT_EQ(t.size(), 1u);
    EXPECT_EQ(t[0], 0x01u);
}

// TC-ENC-2: encodeInt32(0)→{0x00}; (1)→{0x01}; (127)→{0x7F}
TEST(EncodeTest, TC_ENC_2_Int32_Small) {
    auto z = enc([](auto& o){ encodeInt32(0, o); });
    ASSERT_EQ(z.size(), 1u);
    EXPECT_EQ(z[0], 0x00u);

    auto one = enc([](auto& o){ encodeInt32(1, o); });
    ASSERT_EQ(one.size(), 1u);
    EXPECT_EQ(one[0], 0x01u);

    auto s = enc([](auto& o){ encodeInt32(127, o); });
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0], 0x7Fu);
}

// TC-ENC-3: encodeInt32(-1) → 10 bytes (0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0x01)
TEST(EncodeTest, TC_ENC_3_Int32_Negative_One) {
    auto v = enc([](auto& o){ encodeInt32(-1, o); });
    ASSERT_EQ(v.size(), 10u);
    // -1 as uint64 = 0xFFFFFFFFFFFFFFFF → LEB128: 9 × 0xFF, then 0x01
    for (size_t i = 0; i < 9; ++i) {
        EXPECT_EQ(v[i], 0xFFu) << "byte " << i;
    }
    EXPECT_EQ(v[9], 0x01u);

    // Round-trip: decode back
    auto decoded = decodeInt32(v.data(), v.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, -1);
}

// TC-ENC-4: encodeInt32(INT32_MIN) → 10 bytes; round-trip
TEST(EncodeTest, TC_ENC_4_Int32_MIN) {
    auto v = enc([](auto& o){ encodeInt32(std::numeric_limits<int32_t>::min(), o); });
    ASSERT_EQ(v.size(), 10u);

    auto decoded = decodeInt32(v.data(), v.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, std::numeric_limits<int32_t>::min());
}

// TC-ENC-4b: encodeInt32(INT32_MAX) → 4 bytes (positive path, differs from negative 10-byte path)
TEST(EncodeTest, TC_ENC_4b_Int32_MAX) {
    auto v = enc([](auto& o){ encodeInt32(std::numeric_limits<int32_t>::max(), o); });
    // INT32_MAX = 0x7FFFFFFF → LEB128: 0xFF 0xFF 0xFF 0xFF 0x07 (5 bytes as uint64)
    ASSERT_EQ(v.size(), 5u);

    auto decoded = decodeInt32(v.data(), v.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, std::numeric_limits<int32_t>::max());
}

// TC-ENC-5: encodeInt64(INT64_MIN) → 10 bytes; round-trip
TEST(EncodeTest, TC_ENC_5_Int64_MIN) {
    auto v = enc([](auto& o){ encodeInt64(std::numeric_limits<int64_t>::min(), o); });
    ASSERT_EQ(v.size(), 10u);

    auto decoded = decodeInt64(v.data(), v.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, std::numeric_limits<int64_t>::min());
}

// TC-ENC-6: encodeUInt64(UINT64_MAX) → 10 bytes; round-trip
TEST(EncodeTest, TC_ENC_6_UInt64_MAX) {
    auto v = enc([](auto& o){ encodeUInt64(std::numeric_limits<uint64_t>::max(), o); });
    ASSERT_EQ(v.size(), 10u);

    auto decoded = decodeUInt64(v.data(), v.size());
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(*decoded, std::numeric_limits<uint64_t>::max());
}

// TC-ENC-7: encodeFloat edge cases
TEST(EncodeTest, TC_ENC_7_Float_Special) {
    // 0.0f → 4 zero bytes
    auto vz = enc([](auto& o){ encodeFloat(0.0f, o); });
    ASSERT_EQ(vz.size(), 4u);
    for (auto b : vz) EXPECT_EQ(b, 0x00u);

    // NaN round-trip
    float nan_val = std::numeric_limits<float>::quiet_NaN();
    auto vn = enc([nan_val](auto& o){ encodeFloat(nan_val, o); });
    ASSERT_EQ(vn.size(), 4u);
    auto dn = decodeFloat(vn.data(), vn.size());
    ASSERT_TRUE(dn.has_value());
    EXPECT_TRUE(std::isnan(*dn));

    // +Inf round-trip
    float inf_val = std::numeric_limits<float>::infinity();
    auto vi = enc([inf_val](auto& o){ encodeFloat(inf_val, o); });
    ASSERT_EQ(vi.size(), 4u);
    auto di = decodeFloat(vi.data(), vi.size());
    ASSERT_TRUE(di.has_value());
    EXPECT_TRUE(std::isinf(*di) && *di > 0.0f);

    // -0.0f bit-exact round-trip
    float neg_zero = -0.0f;
    auto vno = enc([neg_zero](auto& o){ encodeFloat(neg_zero, o); });
    ASSERT_EQ(vno.size(), 4u);
    auto dno = decodeFloat(vno.data(), vno.size());
    ASSERT_TRUE(dno.has_value());
    // Check bit-exact via memcmp
    uint32_t orig_bits, rt_bits;
    std::memcpy(&orig_bits, &neg_zero, 4);
    std::memcpy(&rt_bits,   &(*dno),   4);
    EXPECT_EQ(orig_bits, rt_bits);
}

// TC-ENC-8: encodeDouble(-Inf) round-trip
TEST(EncodeTest, TC_ENC_8_Double_NegInf) {
    double neg_inf = -std::numeric_limits<double>::infinity();
    auto v = enc([neg_inf](auto& o){ encodeDouble(neg_inf, o); });
    ASSERT_EQ(v.size(), 8u);
    auto d = decodeDouble(v.data(), v.size());
    ASSERT_TRUE(d.has_value());
    EXPECT_TRUE(std::isinf(*d) && *d < 0.0);
}

// TC-ENC-9: encodeString("") → empty; encodeString("hello") → 5 bytes
TEST(EncodeTest, TC_ENC_9_String) {
    auto empty = enc([](auto& o){ encodeString("", o); });
    EXPECT_EQ(empty.size(), 0u);

    auto hello = enc([](auto& o){ encodeString("hello", o); });
    ASSERT_EQ(hello.size(), 5u);
    EXPECT_EQ(hello[0], static_cast<uint8_t>('h'));
    EXPECT_EQ(hello[1], static_cast<uint8_t>('e'));
    EXPECT_EQ(hello[2], static_cast<uint8_t>('l'));
    EXPECT_EQ(hello[3], static_cast<uint8_t>('l'));
    EXPECT_EQ(hello[4], static_cast<uint8_t>('o'));
}

// TC-ENC-10: encodeBytes(4096 zero bytes) → 4096 bytes, all zero
TEST(EncodeTest, TC_ENC_10_Bytes_Large) {
    std::vector<uint8_t> input(4096, 0x00u);
    auto v = enc([&input](auto& o){
        encodeBytes(input.data(), input.size(), o);
    });
    ASSERT_EQ(v.size(), 4096u);
    for (auto b : v) EXPECT_EQ(b, 0x00u);
}

// TC-ENC-11: encode appends (call encodeBool twice on same vector → size==2)
TEST(EncodeTest, TC_ENC_11_Appends) {
    std::vector<uint8_t> out;
    encodeBool(false, out);
    encodeBool(true, out);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], 0x00u);
    EXPECT_EQ(out[1], 0x01u);
}

// TC-ENC-12: varIntSize
TEST(EncodeTest, TC_ENC_12_VarIntSize) {
    EXPECT_EQ(quickstore::detail::varIntSize(0u),       1u);
    EXPECT_EQ(quickstore::detail::varIntSize(127u),     1u);
    EXPECT_EQ(quickstore::detail::varIntSize(128u),     2u);
    EXPECT_EQ(quickstore::detail::varIntSize(16383u),   2u);
    EXPECT_EQ(quickstore::detail::varIntSize(16384u),   3u);
    EXPECT_EQ(quickstore::detail::varIntSize(std::numeric_limits<uint64_t>::max()), 10u);
}
