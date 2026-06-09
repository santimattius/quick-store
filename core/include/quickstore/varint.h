#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace quickstore::detail {

// Reads an unsigned LEB128 varint from [p, end).
// Returns bytes consumed (1..5). Sets error=true and returns 0 on failure.
[[nodiscard]] size_t readUInt32(const uint8_t* p, const uint8_t* end,
                                uint32_t& out, bool& error);

// Reads an unsigned LEB128 varint from [p, end).
// Returns bytes consumed (1..10). Sets error=true and returns 0 on failure.
[[nodiscard]] size_t readUInt64(const uint8_t* p, const uint8_t* end,
                                uint64_t& out, bool& error);

// WHY: MMKV sign-extends negative int32 values to 64-bit before LEB128-encoding.
// A value like int32_t(-1) becomes uint64_t(0xFFFFFFFFFFFFFFFF) = 10 bytes on disk.
// We must read as uint64 and narrow-cast to recover the original int32.
// Returns bytes consumed (1..10). Sets error=true and returns 0 on failure.
[[nodiscard]] size_t readInt32(const uint8_t* p, const uint8_t* end,
                               int32_t& out, bool& error);

// WHY: same sign-extension rule as readInt32, but no narrowing needed.
// Returns bytes consumed (1..10). Sets error=true and returns 0 on failure.
[[nodiscard]] size_t readInt64(const uint8_t* p, const uint8_t* end,
                               int64_t& out, bool& error);

// --- Write path (Fase 1) ---

// Writes unsigned LEB128 varint v into dst. Returns bytes written (1..5).
// PRECONDITION: dst has sufficient space (at least 5 bytes).
[[nodiscard]] size_t writeUInt32(uint8_t* dst, uint32_t v) noexcept;

// Writes unsigned LEB128 varint v into dst. Returns bytes written (1..10).
// PRECONDITION: dst has sufficient space (at least 10 bytes).
[[nodiscard]] size_t writeUInt64(uint8_t* dst, uint64_t v) noexcept;

// WHY: mirrors readInt32 — sign-extends to int64 then to uint64, then LEB128.
// Negative values always encode to exactly 10 bytes (sign extension fills upper bits).
// Returns bytes written (1..10).
// PRECONDITION: dst has sufficient space (at least 10 bytes).
[[nodiscard]] size_t writeInt32(uint8_t* dst, int32_t v) noexcept;

// WHY: mirrors readInt64 — cast to uint64, then LEB128.
// Negative values always encode to exactly 10 bytes.
// Returns bytes written (1..10).
// PRECONDITION: dst has sufficient space (at least 10 bytes).
[[nodiscard]] size_t writeInt64(uint8_t* dst, int64_t v) noexcept;

// Returns how many bytes LEB128 encoding of v would occupy (1..10).
// Does NOT write anything; used for reserve() before bulk encoding.
[[nodiscard]] size_t varIntSize(uint64_t v) noexcept;

} // namespace quickstore::detail
