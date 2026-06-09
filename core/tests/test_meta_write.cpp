#include "quickstore/meta_info.h"
#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
#include <fcntl.h>

using namespace quickstore;

// Test fixture: creates a temporary file and cleans up on teardown.
class MetaWriteTest : public ::testing::Test {
protected:
    int         m_fd   = -1;
    std::string m_path;

    void SetUp() override {
        // mkstemp needs a mutable buffer.
        char tmpl[] = "/tmp/quickstore_meta_test_XXXXXX";
        m_fd = ::mkstemp(tmpl);
        ASSERT_GE(m_fd, 0) << "mkstemp failed";
        m_path = std::string(tmpl);

        // Pre-size the file to exactly kMetaInfoSize bytes.
        ASSERT_EQ(::ftruncate(m_fd, static_cast<off_t>(kMetaInfoSize)), 0)
            << "ftruncate failed";
    }

    void TearDown() override {
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
        if (!m_path.empty()) {
            ::unlink(m_path.c_str());
        }
    }
};

// TC-META-W-1: writeMetaInfo full round-trip — all fields preserved.
TEST_F(MetaWriteTest, TC_META_W_1_FullRoundTrip) {
    MMKVMetaInfo info{};
    info.m_crcDigest             = 0xDEADBEEFu;
    info.m_version               = 4u;
    info.m_sequence              = 7u;
    for (int i = 0; i < 16; ++i) info.m_vector[i] = static_cast<uint8_t>(i + 1);
    info.m_actualSize            = 1024u;
    info.lastConfirmedActualSize = 512u;
    info.lastConfirmedCRCDigest  = 0x12345678u;
    // _reserved stays zero (zero-init in {}), m_flags nonzero for verification.
    info.m_flags                 = 0xCAFEBABECAFEBABEull;

    bool ok = writeMetaInfo(m_fd, info);
    ASSERT_TRUE(ok);

    auto parsed = readMetaInfo(m_path);
    ASSERT_TRUE(parsed.has_value());

    EXPECT_EQ(parsed->m_crcDigest,             info.m_crcDigest);
    EXPECT_EQ(parsed->m_version,               info.m_version);
    EXPECT_EQ(parsed->m_sequence,              info.m_sequence);
    EXPECT_EQ(std::memcmp(parsed->m_vector, info.m_vector, 16), 0);
    EXPECT_EQ(parsed->m_actualSize,            info.m_actualSize);
    EXPECT_EQ(parsed->lastConfirmedActualSize, info.lastConfirmedActualSize);
    EXPECT_EQ(parsed->lastConfirmedCRCDigest,  info.lastConfirmedCRCDigest);
    EXPECT_EQ(parsed->m_flags,                 info.m_flags);
}

// TC-META-W-2: writeMetaInfoFast only touches bytes [0,4) and [28,32).
TEST_F(MetaWriteTest, TC_META_W_2_FastPathOnlyTouchesEightBytes) {
    // First write a known full block.
    MMKVMetaInfo info{};
    info.m_crcDigest             = 0x11111111u;
    info.m_version               = 4u;
    info.m_sequence              = 3u;
    for (int i = 0; i < 16; ++i) info.m_vector[i] = static_cast<uint8_t>(0xAA);
    info.m_actualSize            = 100u;
    info.lastConfirmedActualSize = 50u;
    info.lastConfirmedCRCDigest  = 0x22222222u;
    info.m_flags                 = 0xBBBBBBBBBBBBBBBBull;

    ASSERT_TRUE(writeMetaInfo(m_fd, info));

    // Now fast-update with different CRC and actualSize.
    const uint32_t newCRC  = 0x99999999u;
    const uint32_t newSize = 999u;
    writeMetaInfoFast(m_fd, newCRC, newSize);

    // Raw-read the full 112 bytes and verify.
    uint8_t raw[kMetaInfoSize]{};
    ssize_t n = ::pread(m_fd, raw, kMetaInfoSize, 0);
    ASSERT_EQ(n, static_cast<ssize_t>(kMetaInfoSize));

    // Bytes [0,4) must equal newCRC (little-endian).
    uint32_t readCRC = 0;
    std::memcpy(&readCRC, raw + 0, 4);
    EXPECT_EQ(readCRC, newCRC);

    // Bytes [28,32) must equal newSize (little-endian).
    uint32_t readSize = 0;
    std::memcpy(&readSize, raw + 28, 4);
    EXPECT_EQ(readSize, newSize);

    // All other bytes must be unchanged from the original writeMetaInfo call.
    // Check m_version at [4,8).
    uint32_t readVersion = 0;
    std::memcpy(&readVersion, raw + 4, 4);
    EXPECT_EQ(readVersion, info.m_version);

    // Check m_sequence at [8,12).
    uint32_t readSeq = 0;
    std::memcpy(&readSeq, raw + 8, 4);
    EXPECT_EQ(readSeq, info.m_sequence);

    // Check m_vector at [12,28).
    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(raw[12 + i], 0xAAu) << "m_vector[" << i << "] changed";
    }

    // Check lastConfirmedActualSize at [32,36).
    uint32_t readLCA = 0;
    std::memcpy(&readLCA, raw + 32, 4);
    EXPECT_EQ(readLCA, info.lastConfirmedActualSize);

    // Check lastConfirmedCRCDigest at [36,40).
    uint32_t readLCD = 0;
    std::memcpy(&readLCD, raw + 36, 4);
    EXPECT_EQ(readLCD, info.lastConfirmedCRCDigest);

    // Check m_flags at [104,112).
    uint64_t readFlags = 0;
    std::memcpy(&readFlags, raw + 104, 8);
    EXPECT_EQ(readFlags, info.m_flags);
}

// TC-META-W-3: writeMetaInfo returns false for invalid fd (-1).
TEST(MetaWriteTest_NoFixture, TC_META_W_3_InvalidFd) {
    MMKVMetaInfo info{};
    bool result = writeMetaInfo(-1, info);
    EXPECT_FALSE(result);
}
