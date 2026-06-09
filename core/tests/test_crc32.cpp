#include "quickstore/crc32_validator.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

using namespace quickstore;

// TC-CRC-1: actualSize==0 → CRC = zlib crc32(0, nullptr, 0) = 0
TEST(CRC32, EmptyRange) {
    std::vector<uint8_t> buf(4, 0); // only the 4-byte header, no data
    EXPECT_EQ(computeCRC(buf.data(), 0), 0u);
}

// TC-CRC-2: known test vector
TEST(CRC32, KnownVector) {
    // Buffer: 4 bytes header + 4 bytes data "TEST"
    std::vector<uint8_t> buf = {0,0,0,0, 'T','E','S','T'};
    uint32_t crc = computeCRC(buf.data(), 4);
    // CRC32 of "TEST": computed by zlib, not hardcoded — we verify self-consistency
    EXPECT_EQ(validateCRC(buf.data(), 4, crc), true);
    // Also verify it's not zero (extremely unlikely for non-empty input)
    EXPECT_NE(crc, 0u);
}

// TC-CRC-3: validateCRC returns true when CRC matches
TEST(CRC32, ValidateCRC_Match) {
    std::vector<uint8_t> buf = {0,0,0,0, 0xAA, 0xBB, 0xCC};
    uint32_t crc = computeCRC(buf.data(), 3);
    EXPECT_TRUE(validateCRC(buf.data(), 3, crc));
}

// TC-CRC-4: validateCRC returns false when CRC doesn't match
TEST(CRC32, ValidateCRC_Mismatch) {
    std::vector<uint8_t> buf = {0,0,0,0, 0xAA, 0xBB, 0xCC};
    uint32_t crc = computeCRC(buf.data(), 3);
    EXPECT_FALSE(validateCRC(buf.data(), 3, crc ^ 0x1));
}

// TC-CRC-5: CRC is computed starting at base+4, not base+0
TEST(CRC32, SkipsFirst4Bytes) {
    // Two buffers: same data at [4..], different bytes at [0..3]
    std::vector<uint8_t> buf1 = {0x00, 0x00, 0x00, 0x00, 'D', 'A', 'T', 'A'};
    std::vector<uint8_t> buf2 = {0xFF, 0xFF, 0xFF, 0xFF, 'D', 'A', 'T', 'A'};
    // CRC must be identical (first 4 bytes excluded)
    EXPECT_EQ(computeCRC(buf1.data(), 4), computeCRC(buf2.data(), 4));
}

// TC-CRC-6: boundary — exactly 1 byte of data
TEST(CRC32, SingleByte) {
    std::vector<uint8_t> buf = {0, 0, 0, 0, 0x42};
    uint32_t crc = computeCRC(buf.data(), 1);
    EXPECT_TRUE(validateCRC(buf.data(), 1, crc));
}
