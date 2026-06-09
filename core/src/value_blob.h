#pragma once
#include "quickstore/key_value_holder.h"
#include "quickstore/varint.h"
#include <cstdint>
#include <utility>

namespace quickstore::detail {

// Returns the raw value blob pointer + length for a key.
// Uses explicit varint-skip (canonical path per DD6).
// Extracted from reader.cpp and writer.cpp — both implementations were byte-identical.
[[nodiscard]] static inline
std::pair<const uint8_t*, uint32_t>
getValueBlob(const uint8_t* base, size_t fileSize, const KeyValueHolder& h) noexcept {
    const uint8_t* p        = base + 4 + h.offset;
    const uint8_t* scan_end = base + fileSize;

    uint32_t keyLen = 0;
    bool err = false;
    size_t n = readUInt32(p, scan_end, keyLen, err);
    if (err) return {nullptr, 0};
    p += n + keyLen;

    uint32_t valueLen = 0;
    n = readUInt32(p, scan_end, valueLen, err);
    if (err) return {nullptr, 0};
    p += n;

    return {p, valueLen};
}

} // namespace quickstore::detail
