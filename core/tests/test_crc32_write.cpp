#include "quickstore/crc32_validator.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <vector>
#include <numeric>
#include <zlib.h>

using namespace quickstore;

// TC-CRC-W-1: split-boundary invariant
// updateCRC(updateCRC(0, A, |A|), B, |B|) == crc32(crc32(0,A,|A|), B, |B|)
// which should equal crc32(0, A||B, |A|+|B|).
TEST(CRC32WriteTest, TC_CRC_W_1_SplitBoundary) {
    // Fill 64 bytes with a known pattern.
    uint8_t data[64];
    for (int i = 0; i < 64; ++i) data[i] = static_cast<uint8_t>(i + 1);

    // Part A = data[0..31], Part B = data[32..63]
    uint32_t crcA  = updateCRC(0u, data,      32);
    uint32_t crcAB = updateCRC(crcA, data + 32, 32);

    // Full CRC over all 64 bytes using zlib directly.
    uint32_t full = static_cast<uint32_t>(crc32(0L, data, 64));

    EXPECT_EQ(crcAB, full);
}

// Also verify the same invariant at different split positions and sizes.
TEST(CRC32WriteTest, TC_CRC_W_1_SplitBoundary_Various) {
    for (size_t splitAt : {1u, 50u, 100u}) {
        std::vector<uint8_t> data(200);
        std::iota(data.begin(), data.end(), static_cast<uint8_t>(7));

        uint32_t partA = updateCRC(0u, data.data(), splitAt);
        uint32_t both  = updateCRC(partA, data.data() + splitAt, 200u - splitAt);
        uint32_t full  = static_cast<uint32_t>(crc32(0L, data.data(), 200u));

        EXPECT_EQ(both, full) << "split at " << splitAt;
    }
}

// TC-CRC-W-2: zero-length chunk: updateCRC(prev, ptr, 0) == prev
TEST(CRC32WriteTest, TC_CRC_W_2_ZeroLength) {
    uint8_t dummy[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    const uint32_t prev = 0xDEADBEEFu;

    // Use the dummy buffer pointer but length 0 — no UB.
    uint32_t result = updateCRC(prev, dummy, 0u);
    EXPECT_EQ(result, prev);
}

// TC-CRC-W-3: updateCRC(0, ptr, len) == zlib crc32(0, ptr, len)
TEST(CRC32WriteTest, TC_CRC_W_3_MatchesZlib) {
    std::vector<uint8_t> data(100);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i * 3 + 1);

    uint32_t qsResult   = updateCRC(0u, data.data(), data.size());
    uint32_t zlibResult = static_cast<uint32_t>(crc32(0L, data.data(), static_cast<uInt>(data.size())));

    EXPECT_EQ(qsResult, zlibResult);
}

// TC-CRC-W-4: two-chunk append invariant with known pattern
// Verifies that chunked updateCRC equals full zlib over the concatenation.
TEST(CRC32WriteTest, TC_CRC_W_4_AppendInvariant) {
    // Chunk 1: 4096 bytes of 0xAA
    std::vector<uint8_t> chunk1(4096, 0xAAu);
    // Chunk 2: 4096 bytes of 0x55
    std::vector<uint8_t> chunk2(4096, 0x55u);

    uint32_t crc1 = updateCRC(0u, chunk1.data(), chunk1.size());
    uint32_t crc2 = updateCRC(crc1, chunk2.data(), chunk2.size());

    // Concatenated reference
    std::vector<uint8_t> concat;
    concat.insert(concat.end(), chunk1.begin(), chunk1.end());
    concat.insert(concat.end(), chunk2.begin(), chunk2.end());
    uint32_t expected = static_cast<uint32_t>(crc32(0L, concat.data(), static_cast<uInt>(concat.size())));

    EXPECT_EQ(crc2, expected);
}
