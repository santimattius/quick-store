#include "quickstore/meta_info.h"
#include <gtest/gtest.h>
#include <cstddef>
#include <cstring>

using namespace quickstore;

// TC-META-1: sizeof == 112
TEST(MetaInfo, SizeOf112) {
    EXPECT_EQ(sizeof(MMKVMetaInfo), 112u);
}

// TC-META-2: offsets 0, 4, 8
TEST(MetaInfo, Offsets_CrcVersionSequence) {
    EXPECT_EQ(offsetof(MMKVMetaInfo, m_crcDigest), 0u);
    EXPECT_EQ(offsetof(MMKVMetaInfo, m_version),   4u);
    EXPECT_EQ(offsetof(MMKVMetaInfo, m_sequence),  8u);
}

// TC-META-3: offsets 12, 28
TEST(MetaInfo, Offsets_VectorActualSize) {
    EXPECT_EQ(offsetof(MMKVMetaInfo, m_vector),     12u);
    EXPECT_EQ(offsetof(MMKVMetaInfo, m_actualSize), 28u);
}

// TC-META-4: offsets 32, 36
TEST(MetaInfo, Offsets_LastConfirmed) {
    EXPECT_EQ(offsetof(MMKVMetaInfo, lastConfirmedActualSize), 32u);
    EXPECT_EQ(offsetof(MMKVMetaInfo, lastConfirmedCRCDigest),  36u);
}

// TC-META-5: offset 104
TEST(MetaInfo, Offset_Flags) {
    EXPECT_EQ(offsetof(MMKVMetaInfo, m_flags), 104u);
}

// TC-META-6: parseMetaInfo round-trip with hand-crafted buffer
TEST(MetaInfo, ParseMetaInfo_RoundTrip) {
    uint8_t buf[112] = {};

    uint32_t crc      = 0xDEADBEEF;
    uint32_t version  = 4;
    uint32_t sequence = 7;
    uint8_t  iv[16]   = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    uint32_t actual   = 512;
    uint32_t lcActual = 256;
    uint32_t lcCRC    = 0xCAFEBABE;
    uint64_t flags    = 0x0000000000000001ULL;

    std::memcpy(buf + 0,   &crc,      4);
    std::memcpy(buf + 4,   &version,  4);
    std::memcpy(buf + 8,   &sequence, 4);
    std::memcpy(buf + 12,  iv,       16);
    std::memcpy(buf + 28,  &actual,   4);
    std::memcpy(buf + 32,  &lcActual, 4);
    std::memcpy(buf + 36,  &lcCRC,    4);
    std::memcpy(buf + 104, &flags,    8);

    auto result = parseMetaInfo(buf, 112);
    ASSERT_TRUE(result.has_value());
    const MMKVMetaInfo& info = *result;

    EXPECT_EQ(info.m_crcDigest,             crc);
    EXPECT_EQ(info.m_version,               version);
    EXPECT_EQ(info.m_sequence,              sequence);
    EXPECT_EQ(std::memcmp(info.m_vector, iv, 16), 0);
    EXPECT_EQ(info.m_actualSize,            actual);
    EXPECT_EQ(info.lastConfirmedActualSize, lcActual);
    EXPECT_EQ(info.lastConfirmedCRCDigest,  lcCRC);
    EXPECT_EQ(info.m_flags,                 flags);
}

// TC-META-7: readMetaInfo with non-existent path returns nullopt
TEST(MetaInfo, ReadMetaInfo_NonExistentPath) {
    auto result = readMetaInfo("/tmp/quickstore_nonexistent_test_file_xyz.crc");
    EXPECT_FALSE(result.has_value());
}

// TC-META-8: parseMetaInfo with buffer shorter than 112 bytes returns nullopt
TEST(MetaInfo, ParseMetaInfo_ShortBuffer) {
    uint8_t buf[50] = {};
    EXPECT_FALSE(parseMetaInfo(buf, 50).has_value());
    EXPECT_FALSE(parseMetaInfo(buf, 0).has_value());
    EXPECT_FALSE(parseMetaInfo(buf, 111).has_value());
}
