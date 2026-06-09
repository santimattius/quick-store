#pragma once
#include "quickstore/key_value_holder.h"
#include "quickstore/varint.h"
#include <cstdint>
#include <string>
#include <unordered_map>

namespace quickstore {

// Reads bytes [0,4) as a uint32 LE via memcpy.
// ONLY for the downgrade path in checkDataValid (m_version < 3).
[[nodiscard]] uint32_t readOldStyleActualSize(const uint8_t* base);

// Decodes the key-value map from the data file region [base+4, base+4+actualSize).
// ItemSizeHolder is skipped as FIXED 4 bytes (base+4..base+8); scan starts at base+8.
// greedy=false (default): abort on first malformed record, return partial map.
// greedy=true: skip corrupt bytes and continue best-effort; never returns an error.
[[nodiscard]] std::unordered_map<std::string, KeyValueHolder>
decodeMap(const uint8_t* base, uint32_t actualSize, bool greedy = false);

// Fixed 4-byte ItemSizeHolder size constant.
constexpr uint32_t ItemSizeHolderSize = 4;

} // namespace quickstore
