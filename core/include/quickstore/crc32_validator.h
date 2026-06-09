#pragma once
#include <cstdint>
#include <cstddef>

namespace quickstore {

// Computes CRC32 over [base+4, base+4+actualSize) using zlib (polynomial 0xEDB88320).
// actualSize==0 returns 0 (CRC over empty range).
// WHY: the first 4 bytes of the data file are the header/size; CRC covers only payload.
[[nodiscard]] uint32_t computeCRC(const uint8_t* base, uint32_t actualSize);

// Returns true iff computeCRC(base, actualSize) == expected.
[[nodiscard]] bool validateCRC(const uint8_t* base, uint32_t actualSize, uint32_t expected);

// Resumes an incremental CRC32 digest.
// Equivalent to crc32(prev, ptr, len) from zlib.
// WHY: append-path extends the rolling digest from the prior value without rescanning
// the entire buffer. len==0 returns prev unchanged (zlib guarantees this).
[[nodiscard]] uint32_t updateCRC(uint32_t prev, const uint8_t* ptr, size_t len) noexcept;

} // namespace quickstore
