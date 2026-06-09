#include "quickstore/varint.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <limits>

using namespace quickstore::detail;

// Helper: write v into buf, then read it back. Returns bytes consumed by read.
// Also asserts that writeCount == readCount.
template<typename WriteV, typename ReadFn>
static size_t roundtrip(uint8_t* buf, size_t bufSize, WriteV writeCount, ReadFn readFn) {
    (void)bufSize;
    bool error = false;
    size_t consumed = readFn(buf, buf + writeCount, error);
    EXPECT_FALSE(error);
    EXPECT_EQ(consumed, writeCount);
    return consumed;
}

// TC-VAR-W-1: writeUInt32 then readUInt32 round-trips
TEST(VarintWriteTest, TC_VAR_W_1_UInt32) {
    const uint32_t cases[] = { 0u, 127u, 128u, 16383u,
                                std::numeric_limits<uint32_t>::max() };
    for (uint32_t v : cases) {
        uint8_t buf[16]{};
        size_t written = writeUInt32(buf, v);
        EXPECT_EQ(written, varIntSize(static_cast<uint64_t>(v)))
            << "varIntSize mismatch for v=" << v;

        uint32_t out = 0;
        bool error = false;
        size_t consumed = readUInt32(buf, buf + written, out, error);
        EXPECT_FALSE(error) << "error for v=" << v;
        EXPECT_EQ(consumed, written) << "consumed != written for v=" << v;
        EXPECT_EQ(out, v) << "round-trip failed for v=" << v;
    }
}

// TC-VAR-W-2: writeUInt64 then readUInt64 round-trips
TEST(VarintWriteTest, TC_VAR_W_2_UInt64) {
    const uint64_t cases[] = { 0u, std::numeric_limits<uint64_t>::max() };
    for (uint64_t v : cases) {
        uint8_t buf[16]{};
        size_t written = writeUInt64(buf, v);
        EXPECT_EQ(written, varIntSize(v)) << "varIntSize mismatch for v=" << v;

        uint64_t out = 0;
        bool error = false;
        size_t consumed = readUInt64(buf, buf + written, out, error);
        EXPECT_FALSE(error);
        EXPECT_EQ(consumed, written);
        EXPECT_EQ(out, v);
    }
}

// TC-VAR-W-3: writeInt32(-1) → 10 bytes; readInt32 → -1
TEST(VarintWriteTest, TC_VAR_W_3_Int32_Negative) {
    uint8_t buf[16]{};
    size_t written = writeInt32(buf, -1);
    EXPECT_EQ(written, 10u);  // sign extension fills all 64 bits

    int32_t out = 0;
    bool error = false;
    size_t consumed = readInt32(buf, buf + written, out, error);
    EXPECT_FALSE(error);
    EXPECT_EQ(consumed, 10u);
    EXPECT_EQ(out, -1);
}

// TC-VAR-W-4: writeInt64(INT64_MIN) → 10 bytes; readInt64 → INT64_MIN
TEST(VarintWriteTest, TC_VAR_W_4_Int64_MIN) {
    uint8_t buf[16]{};
    int64_t val = std::numeric_limits<int64_t>::min();
    size_t written = writeInt64(buf, val);
    EXPECT_EQ(written, 10u);

    int64_t out = 0;
    bool error = false;
    size_t consumed = readInt64(buf, buf + written, out, error);
    EXPECT_FALSE(error);
    EXPECT_EQ(consumed, 10u);
    EXPECT_EQ(out, val);
}

// TC-VAR-W-5: varIntSize matches actual bytes written for representative values
TEST(VarintWriteTest, TC_VAR_W_5_VarIntSize) {
    const uint64_t cases[] = {
        0u,
        127u,
        128u,
        16384u,
        std::numeric_limits<uint64_t>::max()
    };
    for (uint64_t v : cases) {
        uint8_t buf[16]{};
        size_t written = writeUInt64(buf, v);
        size_t sz = varIntSize(v);
        EXPECT_EQ(written, sz) << "varIntSize(" << v << ") = " << sz
                               << " but writeUInt64 wrote " << written << " bytes";
    }
}
