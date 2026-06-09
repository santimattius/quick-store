#include "quickstore/writer.h"
#include "quickstore/reader.h"
#include "quickstore/crc32_validator.h"
#include "quickstore/meta_info.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct TempDirMeta {
    std::string path;
    TempDirMeta() {
        char tmpl[] = "/tmp/qs_meta_XXXXXX";
        const char* d = ::mkdtemp(tmpl);
        EXPECT_NE(d, nullptr);
        path = d ? d : "";
    }
    ~TempDirMeta() { if (!path.empty()) fs::remove_all(path); }
};

// TC-META-W-4: after setInt32, raw-read .crc bytes [0,4) == CRC32 of data region
TEST(WriterMeta, TC_META_W_4_CRCInCrcFile) {
    TempDirMeta tmp;
    const std::string id = "crc_check";
    {
        auto w = quickstore::QuickStoreWriter::open(id, tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("x", 12345));
        w->close();
    }

    // Read crc file
    std::string crcPath = tmp.path + "/" + id + ".crc";
    auto meta = quickstore::readMetaInfo(crcPath);
    ASSERT_TRUE(meta.has_value());

    // Verify using a fresh Reader — if the reader can open, CRC is valid
    auto r = quickstore::QuickStoreReader::open(id, tmp.path);
    ASSERT_TRUE(r.has_value()) << "CRC validation failed — Reader couldn't open file";

    // Raw-verify: the stored CRC must match what computeCRC would produce over the data file
    std::string dataPath = tmp.path + "/" + id;
    int fd = ::open(dataPath.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    struct stat st{};
    ::fstat(fd, &st);
    std::vector<uint8_t> buf(static_cast<size_t>(st.st_size));
    auto nread = ::read(fd, buf.data(), buf.size());
    (void)nread;
    ::close(fd);

    uint32_t expectedCRC = quickstore::computeCRC(buf.data(), meta->m_actualSize);
    EXPECT_EQ(meta->m_crcDigest, expectedCRC);
}

// TC-META-W-5: after setInt32, raw-read .crc bytes [28,32) == m_actualSize
TEST(WriterMeta, TC_META_W_5_ActualSizeInCrcFile) {
    TempDirMeta tmp;
    const std::string id = "size_check";
    {
        auto w = quickstore::QuickStoreWriter::open(id, tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("x", 999));
        w->close();
    }

    std::string crcPath = tmp.path + "/" + id + ".crc";
    int fd = ::open(crcPath.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    uint32_t storedActualSize = 0;
    ::pread(fd, &storedActualSize, 4, 28);
    ::close(fd);

    // actualSize must be > 4 (ItemSizeHolder) since we wrote at least one record
    EXPECT_GT(storedActualSize, 4u);

    // Also verify via readMetaInfo
    auto meta = quickstore::readMetaInfo(crcPath);
    ASSERT_TRUE(meta.has_value());
    EXPECT_EQ(meta->m_actualSize, storedActualSize);
}

// TC-CRC-W-4: after compact(), stored m_crcDigest == computeCRC(base, m_actualSize) with seed 0
TEST(WriterMeta, TC_CRC_W_4_CRCAfterWriteback) {
    TempDirMeta tmp;
    const std::string id = "crc_wb";
    {
        auto w = quickstore::QuickStoreWriter::open(id, tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("a", 1));
        EXPECT_TRUE(w->setString("b", "hello"));
        EXPECT_TRUE(w->compact()); // triggers full write-back with seed 0 CRC
        w->close();
    }

    std::string crcPath  = tmp.path + "/" + id + ".crc";
    std::string dataPath = tmp.path + "/" + id;

    auto meta = quickstore::readMetaInfo(crcPath);
    ASSERT_TRUE(meta.has_value());

    // Read full data file
    int fd = ::open(dataPath.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    struct stat st{};
    ::fstat(fd, &st);
    std::vector<uint8_t> buf(static_cast<size_t>(st.st_size));
    ::read(fd, buf.data(), buf.size());
    ::close(fd);

    // After writeback, CRC is computeCRC(base, m_actualSize) = crc32(0, base+4, actualSize)
    uint32_t expectedCRC = quickstore::computeCRC(buf.data(), meta->m_actualSize);
    EXPECT_EQ(meta->m_crcDigest, expectedCRC)
        << "Post-writeback CRC must equal computeCRC(base, actualSize) with seed 0";
    // lastConfirmed must match too
    EXPECT_EQ(meta->lastConfirmedCRCDigest,   meta->m_crcDigest);
    EXPECT_EQ(meta->lastConfirmedActualSize,   meta->m_actualSize);
}

// TC-META-W-6: raw-read .crc [4,8)==m_version==4, [8,12)==m_sequence==0 initially
TEST(WriterMeta, TC_META_W_6_VersionAndSequenceInitial) {
    TempDirMeta tmp;
    const std::string id = "version_seq";
    {
        auto w = quickstore::QuickStoreWriter::open(id, tmp.path);
        ASSERT_TRUE(w.has_value());
        // Do NOT write any records — just open and close
        // so sequence stays 0 (no full writeback triggered)
        w->close();
    }

    std::string crcPath = tmp.path + "/" + id + ".crc";
    int fd = ::open(crcPath.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    uint32_t version = 0, sequence = 0;
    ::pread(fd, &version,  4, 4);
    ::pread(fd, &sequence, 4, 8);
    ::close(fd);

    EXPECT_EQ(version,  4u);
    EXPECT_EQ(sequence, 0u);
}
