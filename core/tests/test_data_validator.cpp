#include "quickstore/data_validator.h"
#include "quickstore/crc32_validator.h"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

using namespace quickstore;

// Build a minimal valid data buffer: 4 bytes oldStyleActualSize + 4 bytes ItemSizeHolder
static std::vector<uint8_t> makeDataBuf(uint32_t extraBytes = 0) {
    std::vector<uint8_t> buf(8 + extraBytes, 0xAA);
    // oldStyleActualSize = 4 + extraBytes (LE)
    uint32_t oas = 4 + extraBytes;
    std::memcpy(buf.data(), &oas, 4);
    return buf;
}

static MMKVMetaInfo makeValidMeta(const std::vector<uint8_t>& buf, uint32_t actualSize, uint32_t version = 4) {
    MMKVMetaInfo meta{};
    meta.m_version   = version;
    meta.m_actualSize = actualSize;
    meta.m_crcDigest = computeCRC(buf.data(), actualSize);
    // lastConfirmed mirrors primary for simplicity
    meta.lastConfirmedActualSize = actualSize;
    meta.lastConfirmedCRCDigest  = meta.m_crcDigest;
    return meta;
}

// TC-VAL-1: valid primary path → loadFromFile=true, needFullWriteback=false
TEST(DataValidator, ValidPrimary) {
    auto buf = makeDataBuf();
    uint32_t actualSize = 4; // just ItemSizeHolder
    auto meta = makeValidMeta(buf, actualSize);
    auto r = checkDataValid(buf.data(), buf.size(), meta, RecoverStrategy::Discard);
    EXPECT_TRUE(r.loadFromFile);
    EXPECT_FALSE(r.needFullWriteback);
    EXPECT_EQ(r.effectiveActualSize, actualSize);
}

// TC-VAL-2: truncated file (fileSize < actualSize+4) → falls through to lastConfirmed
TEST(DataValidator, TruncatedFile_UsesLastConfirmed) {
    auto buf = makeDataBuf();
    uint32_t actualSize = 4;
    auto meta = makeValidMeta(buf, actualSize);
    meta.m_actualSize = 9999; // claim larger than actual file
    // lastConfirmed has correct smaller size
    auto r = checkDataValid(buf.data(), buf.size(), meta, RecoverStrategy::Discard);
    // Primary truncation check fails; lastConfirmed should work
    EXPECT_TRUE(r.loadFromFile);
    EXPECT_TRUE(r.needFullWriteback);
    EXPECT_EQ(r.effectiveActualSize, actualSize);
}

// TC-VAL-3: bad primary CRC, good lastConfirmed → uses lastConfirmed
TEST(DataValidator, BadPrimaryCRC_GoodLastConfirmed) {
    auto buf = makeDataBuf();
    uint32_t actualSize = 4;
    auto meta = makeValidMeta(buf, actualSize);
    meta.m_crcDigest ^= 0x1; // corrupt primary CRC
    auto r = checkDataValid(buf.data(), buf.size(), meta, RecoverStrategy::Discard);
    EXPECT_TRUE(r.loadFromFile);
    EXPECT_TRUE(r.needFullWriteback);
    EXPECT_EQ(r.effectiveActualSize, actualSize);
}

// TC-VAL-4: both CRCs bad, RecoverStrategy::Recover → loadFromFile=true, needFullWriteback=true
TEST(DataValidator, BothCRCsBad_Recover) {
    auto buf = makeDataBuf();
    uint32_t actualSize = 4;
    auto meta = makeValidMeta(buf, actualSize);
    meta.m_crcDigest             ^= 0x1;
    meta.lastConfirmedCRCDigest  ^= 0x1;
    auto r = checkDataValid(buf.data(), buf.size(), meta, RecoverStrategy::Recover);
    EXPECT_TRUE(r.loadFromFile);
    EXPECT_TRUE(r.needFullWriteback);
}

// TC-VAL-5: both CRCs bad, RecoverStrategy::Discard → loadFromFile=false
TEST(DataValidator, BothCRCsBad_Discard) {
    auto buf = makeDataBuf();
    uint32_t actualSize = 4;
    auto meta = makeValidMeta(buf, actualSize);
    meta.m_crcDigest             ^= 0x1;
    meta.lastConfirmedCRCDigest  ^= 0x1;
    auto r = checkDataValid(buf.data(), buf.size(), meta, RecoverStrategy::Discard);
    EXPECT_FALSE(r.loadFromFile);
}

// TC-VAL-6: m_version < 3 → reads oldStyleActualSize from bytes [0,4)
TEST(DataValidator, OldStyleActualSize_Version2) {
    auto buf = makeDataBuf();
    uint32_t actualSize = 4;
    uint32_t oas = actualSize; // bytes [0,4) = 4 LE
    std::memcpy(buf.data(), &oas, 4);

    auto meta = makeValidMeta(buf, actualSize, /*version=*/2);
    meta.m_actualSize = 9999; // authoritative field should be ignored for version < 3
    // For version 2, meta.m_actualSize is used (it's >= 3 check in our impl uses version >= 3)
    // Our impl: version < 3 → use oldStyleActualSize from bytes [0,4)
    auto r = checkDataValid(buf.data(), buf.size(), meta, RecoverStrategy::Discard);
    // With version=2, candidateActualSize = oldStyleActualSize = 4
    // CRC over [4,8) matches → loadFromFile=true
    EXPECT_TRUE(r.loadFromFile);
    EXPECT_EQ(r.effectiveActualSize, actualSize);
}

// TC-VAL-7: m_version < 2 → no lastConfirmed path available
TEST(DataValidator, Version1_NoLastConfirmed) {
    auto buf = makeDataBuf();
    uint32_t actualSize = 4;
    uint32_t oas = actualSize;
    std::memcpy(buf.data(), &oas, 4);

    MMKVMetaInfo meta{};
    meta.m_version   = 1;
    meta.m_actualSize = 9999; // ignored for version < 3
    meta.m_crcDigest = computeCRC(buf.data(), actualSize) ^ 0x1; // corrupt primary
    // lastConfirmed is zero (version 1 doesn't have it)
    auto r = checkDataValid(buf.data(), buf.size(), meta, RecoverStrategy::Discard);
    // version=1 < 2, so lastConfirmed branch is skipped
    // primary CRC fails → Discard
    EXPECT_FALSE(r.loadFromFile);
}
