#include "quickstore/data_parser.h"
#include <gtest/gtest.h>
#include <cstring>
#include <vector>

using namespace quickstore;

// Helper: build a minimal data buffer (oldStyleActualSize + ItemSizeHolder + records)
// Returns buffer; actualSize = buf.size() - 4
struct DataBuffer {
    std::vector<uint8_t> buf;
    uint32_t actualSize() const { return static_cast<uint32_t>(buf.size() - 4); }
    const uint8_t* base() const { return buf.data(); }
};

static void appendVarint(std::vector<uint8_t>& buf, uint32_t v) {
    while (v & ~0x7Fu) {
        buf.push_back(static_cast<uint8_t>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    buf.push_back(static_cast<uint8_t>(v & 0x7F));
}

static DataBuffer makeBuffer(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& records) {
    DataBuffer db;
    // oldStyleActualSize placeholder (4 bytes LE)
    db.buf.insert(db.buf.end(), {0, 0, 0, 0});
    // ItemSizeHolder (fixed 4 bytes, value ignored on read)
    db.buf.insert(db.buf.end(), {0, 0, 0, 0});
    for (const auto& [key, value] : records) {
        appendVarint(db.buf, static_cast<uint32_t>(key.size()));
        db.buf.insert(db.buf.end(), key.begin(), key.end());
        appendVarint(db.buf, static_cast<uint32_t>(value.size()));
        db.buf.insert(db.buf.end(), value.begin(), value.end());
    }
    return db;
}

// TC-MAP-1: empty map (actualSize==4, only ItemSizeHolder)
TEST(DecodeMap, EmptyMap) {
    DataBuffer db = makeBuffer({});
    auto map = decodeMap(db.base(), db.actualSize());
    EXPECT_TRUE(map.empty());
}

// TC-MAP-2: single key-value
TEST(DecodeMap, SingleEntry) {
    DataBuffer db = makeBuffer({{"hello", {0x01}}});
    auto map = decodeMap(db.base(), db.actualSize());
    ASSERT_EQ(map.size(), 1u);
    ASSERT_TRUE(map.count("hello"));
    EXPECT_EQ(map["hello"].valueSize, 1u);
    EXPECT_EQ(map["hello"].keySize,   5u);
}

// TC-MAP-3: same key written twice → last value wins
TEST(DecodeMap, Upsert_LastWins) {
    std::vector<uint8_t> val1 = {0x01};
    std::vector<uint8_t> val2 = {0x63}; // 99
    DataBuffer db = makeBuffer({{"k", val1}, {"k", val2}});
    auto map = decodeMap(db.base(), db.actualSize());
    ASSERT_EQ(map.size(), 1u);
    ASSERT_TRUE(map.count("k"));
    EXPECT_EQ(map["k"].valueSize, 1u);
    // Verify offset points to the second record (val2), not the first
    const uint8_t* valPtr = db.base() + 4 + map["k"].offset;
    // skip keyLen varint + key + valueLen varint
    uint32_t kl = 0; bool err = false;
    size_t n = detail::readUInt32(valPtr, db.base() + db.buf.size(), kl, err);
    valPtr += n + kl;
    uint32_t vl = 0;
    n = detail::readUInt32(valPtr, db.base() + db.buf.size(), vl, err);
    valPtr += n;
    EXPECT_EQ(*valPtr, 0x63u);
}

// TC-MAP-4: valueLen==0 (tombstone) → key not in map
TEST(DecodeMap, Tombstone_KeyAbsent) {
    DataBuffer db = makeBuffer({{"gone", {}}});
    auto map = decodeMap(db.base(), db.actualSize());
    EXPECT_TRUE(map.empty());
    EXPECT_FALSE(map.count("gone"));
}

// TC-MAP-5: write then delete
TEST(DecodeMap, Tombstone_AfterWrite) {
    DataBuffer db = makeBuffer({{"k", {0x07}}, {"k", {}}});
    auto map = decodeMap(db.base(), db.actualSize());
    EXPECT_TRUE(map.empty());
}

// TC-MAP-6: multiple different keys
TEST(DecodeMap, MultipleKeys) {
    DataBuffer db = makeBuffer({{"a", {0x01}}, {"b", {0x02}}, {"c", {0x03}}});
    auto map = decodeMap(db.base(), db.actualSize());
    EXPECT_EQ(map.size(), 3u);
    EXPECT_TRUE(map.count("a"));
    EXPECT_TRUE(map.count("b"));
    EXPECT_TRUE(map.count("c"));
}

// TC-MAP-7: readOldStyleActualSize reads bytes [0,4) as uint32 LE
TEST(DecodeMap, ReadOldStyleActualSize) {
    uint8_t buf[8] = {0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0};
    EXPECT_EQ(readOldStyleActualSize(buf), 0x12345678u);
}

// TC-MAP-8: greedy=true continues past malformed record
TEST(DecodeMap, Greedy_SkipsCorrupt) {
    // Build a valid record, inject garbage, then another valid record
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {0, 0, 0, 0}); // oldStyleActualSize
    buf.insert(buf.end(), {0, 0, 0, 0}); // ItemSizeHolder
    // Valid record: key="ok", value=1
    appendVarint(buf, 2); buf.push_back('o'); buf.push_back('k');
    appendVarint(buf, 1); buf.push_back(0x01);
    // Garbage byte that looks like a huge keyLen (will fail to read key)
    buf.push_back(0xFF); // partial continuation varint with no terminator
    // Valid record: key="x", value=2
    appendVarint(buf, 1); buf.push_back('x');
    appendVarint(buf, 1); buf.push_back(0x02);

    uint32_t actualSize = static_cast<uint32_t>(buf.size()) - 4;
    auto map = decodeMap(buf.data(), actualSize, /*greedy=*/true);
    // "ok" must be present; "x" may or may not be (greedy best-effort)
    EXPECT_TRUE(map.count("ok"));
}
