#include "quickstore/writer.h"
#include "quickstore/reader.h"
#include "quickstore/crc32_validator.h"
#include "quickstore/meta_info.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

struct TempDirWB {
    std::string path;
    TempDirWB() {
        char tmpl[] = "/tmp/qs_wb_XXXXXX";
        const char* d = ::mkdtemp(tmpl);
        EXPECT_NE(d, nullptr);
        path = d ? d : "";
    }
    ~TempDirWB() { if (!path.empty()) fs::remove_all(path); }
};

// TC-WBK-1: fill > 4096 bytes to trigger grow, verify all keys readable via Reader
TEST(WriterFullWriteback, TC_WBK_1_GrowAndReadBack) {
    TempDirWB tmp;
    const int N = 100;
    {
        auto w = quickstore::QuickStoreWriter::open("grow_test", tmp.path);
        ASSERT_TRUE(w.has_value());
        for (int i = 0; i < N; ++i) {
            std::string key = "key_" + std::to_string(i);
            // Each string value is 40 bytes — N*40 = 4000 + overhead > 4096
            EXPECT_TRUE(w->setString(key, std::string(40, 'A' + (i % 26))));
        }
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("grow_test", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->count(), static_cast<size_t>(N));
    for (int i = 0; i < N; ++i) {
        std::string key = "key_" + std::to_string(i);
        EXPECT_TRUE(r->contains(key)) << "missing key: " << key;
    }
}

// TC-WBK-2: after compact(), m_sequence incremented in .crc
TEST(WriterFullWriteback, TC_WBK_2_SequenceIncrementedAfterWriteback) {
    TempDirWB tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("seq_test", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("k", 42));
        EXPECT_TRUE(w->compact()); // explicit compaction → m_sequence++
        w->close();
    }
    std::string crcPath = tmp.path + "/seq_test.crc";
    auto meta = quickstore::readMetaInfo(crcPath);
    ASSERT_TRUE(meta.has_value());
    EXPECT_GE(meta->m_sequence, 1u) << "m_sequence must be >= 1 after full write-back";
    EXPECT_EQ(meta->lastConfirmedActualSize, meta->m_actualSize);
    EXPECT_EQ(meta->lastConfirmedCRCDigest,  meta->m_crcDigest);

    auto r = quickstore::QuickStoreReader::open("seq_test", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->getInt32("k").value_or(-1), 42);
}

// TC-WBK-3: write key, remove key, write many more keys — removed key absent from Reader
TEST(WriterFullWriteback, TC_WBK_3_RemovedKeyAbsentAfterManyWrites) {
    TempDirWB tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("tombstone", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setString("victim", "delete_me"));
        EXPECT_TRUE(w->remove("victim"));
        // Write enough to potentially trigger writeback
        for (int i = 0; i < 50; ++i) {
            EXPECT_TRUE(w->setString("key_" + std::to_string(i), std::string(30, 'Z')));
        }
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("tombstone", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_FALSE(r->contains("victim"));
    EXPECT_EQ(r->count(), 50u);
}

// TC-WBK-4: Reader opens Writer-produced file cleanly (no validation error)
TEST(WriterFullWriteback, TC_WBK_4_ReaderOpensCleanly) {
    TempDirWB tmp;
    {
        auto w = quickstore::QuickStoreWriter::open("clean_open", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("answer", 42));
        EXPECT_TRUE(w->setString("name", "quickstore"));
        w->close();
    }
    auto r = quickstore::QuickStoreReader::open("clean_open", tmp.path);
    ASSERT_TRUE(r.has_value()) << "Reader failed to open Writer-produced file";
    EXPECT_EQ(r->count(), 2u);
    EXPECT_EQ(r->getInt32("answer").value_or(-1), 42);
    EXPECT_EQ(r->getString("name").value_or(""), "quickstore");
}

// TC-WBK-5: ItemSizeHolder bytes [4,8) change after compact()
TEST(WriterFullWriteback, TC_WBK_5_ItemSizeHolderRegeneratedAfterWriteback) {
    TempDirWB tmp;
    std::string dataPath = tmp.path + "/holder_test";

    // Capture bytes [4,8) before compact
    uint8_t before[4] = {};
    {
        auto w = quickstore::QuickStoreWriter::open("holder_test", tmp.path);
        ASSERT_TRUE(w.has_value());
        EXPECT_TRUE(w->setInt32("x", 99));

        // Read current ItemSizeHolder before compact
        int fd = ::open(dataPath.c_str(), O_RDONLY);
        ASSERT_GE(fd, 0);
        ::pread(fd, before, 4, 4);
        ::close(fd);

        EXPECT_TRUE(w->compact());
        w->close();
    }

    // Read bytes [4,8) after compact — must differ (new random seed)
    uint8_t after[4] = {};
    int fd = ::open(dataPath.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);
    ::pread(fd, after, 4, 4);
    ::close(fd);

    // The ItemSizeHolder is regenerated via arc4random_buf — almost certainly different
    // (probability of collision is 1/2^32, negligible)
    EXPECT_NE(std::memcmp(before, after, 4), 0)
        << "ItemSizeHolder must be regenerated after full write-back";

    auto r = quickstore::QuickStoreReader::open("holder_test", tmp.path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->getInt32("x").value_or(-1), 99);
}
