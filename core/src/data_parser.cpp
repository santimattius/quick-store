#include "quickstore/data_parser.h"
#include <cstring>

namespace quickstore {

uint32_t readOldStyleActualSize(const uint8_t* base) {
    uint32_t result = 0;
    std::memcpy(&result, base, 4);
    return result;
}

std::unordered_map<std::string, KeyValueHolder>
decodeMap(const uint8_t* base, uint32_t actualSize, bool greedy) {
    std::unordered_map<std::string, KeyValueHolder> map;

    if (actualSize < ItemSizeHolderSize) {
        return map;
    }

    // base+4 is the start of the content area (skipping the first 4-byte file header).
    // WHY: ItemSizeHolder is written as a fixed 4-byte LE uint32 at base+4..base+8.
    // Its value is discarded on read; we unconditionally skip exactly 4 bytes.
    const uint8_t* start = base + 4;
    const uint8_t* end   = start + actualSize;

    // Skip ItemSizeHolder (fixed 4 bytes).
    const uint8_t* p = start + ItemSizeHolderSize;

    while (p < end) {
        const uint8_t* record_start = p;
        uint32_t keyLen = 0;
        bool error = false;

        size_t n = detail::readUInt32(p, end, keyLen, error);
        if (error) {
            if (greedy) { ++p; continue; }
            break;
        }
        p += n;

        if (p + keyLen > end) {
            if (greedy) { p = record_start + 1; continue; }
            break;
        }

        std::string key(reinterpret_cast<const char*>(p), keyLen);
        p += keyLen;

        uint32_t valueLen = 0;
        size_t m = detail::readUInt32(p, end, valueLen, error);
        if (error) {
            if (greedy) { p = record_start + 1; continue; }
            break;
        }
        p += m;

        if (p + valueLen > end) {
            if (greedy) { p = record_start + 1; continue; }
            break;
        }

        if (valueLen == 0) {
            // Tombstone: remove the key.
            map.erase(key);
        } else {
            KeyValueHolder holder{};
            holder.offset = static_cast<uint32_t>(record_start - start);
            holder.keySize = static_cast<uint16_t>(keyLen);
            holder.valueSize = valueLen;
            // computedKVSize = varint(keyLen) bytes + keyLen + varint(valueLen) bytes + valueLen
            uint32_t kvSize = static_cast<uint32_t>(n) + keyLen + static_cast<uint32_t>(m) + valueLen;
            holder.computedKVSize = static_cast<uint16_t>(kvSize > 0xFFFF ? 0xFFFF : kvSize);
            map[key] = holder;
        }

        p += valueLen;
    }

    return map;
}

} // namespace quickstore
