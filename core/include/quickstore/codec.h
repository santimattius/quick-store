#pragma once
#include "quickstore/mmbuffer.h"
#include "quickstore/varint.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quickstore {

// --- Decode path (Fase 0) ---

/* Decode a single value blob into its typed representation.
   `blob`/`len` describe the raw value bytes for ONE key (no length prefix).
   Returns nullopt when the bytes are malformed for the requested type. */

// len must be exactly 1 and the byte must be 0x00 (false) or 0x01 (true); else nullopt.
[[nodiscard]] std::optional<bool>     decodeBool(const uint8_t* blob, size_t len);
// LEB128 varint sign-extended to 64-bit then narrowed (MMKV encoding). nullopt if len==0, varint malformed, or it does not consume exactly len bytes.
[[nodiscard]] std::optional<int32_t>  decodeInt32(const uint8_t* blob, size_t len);
// LEB128 varint (MMKV sign-extension). nullopt if len==0, malformed, or not consuming exactly len bytes.
[[nodiscard]] std::optional<int64_t>  decodeInt64(const uint8_t* blob, size_t len);
// Unsigned LEB128 varint. nullopt if len==0, malformed, or not consuming exactly len bytes.
[[nodiscard]] std::optional<uint32_t> decodeUInt32(const uint8_t* blob, size_t len);
// Unsigned LEB128 varint. nullopt if len==0, malformed, or not consuming exactly len bytes.
[[nodiscard]] std::optional<uint64_t> decodeUInt64(const uint8_t* blob, size_t len);
// Raw IEEE-754, not varint. len must be exactly 4; else nullopt.
[[nodiscard]] std::optional<float>    decodeFloat(const uint8_t* blob, size_t len);
// Raw IEEE-754, not varint. len must be exactly 8; else nullopt.
[[nodiscard]] std::optional<double>   decodeDouble(const uint8_t* blob, size_t len);
// Copies [blob, blob+len) verbatim (UTF-8 bytes). Never fails; len==0 yields an empty string.
[[nodiscard]] std::optional<std::string> decodeString(const uint8_t* blob, size_t len);
// Wraps [blob, blob+len) into an owning MMBuffer. Never fails; len==0 yields an empty buffer.
[[nodiscard]] std::optional<MMBuffer> decodeBytes(const uint8_t* blob, size_t len);

// --- Encode path (Fase 1) ---
// All functions APPEND to `out` (do not clear it) and return bytes appended.

// Appends 1 byte: 0x01 (true) or 0x00 (false).
[[nodiscard]] size_t encodeBool(bool value, std::vector<uint8_t>& out);
// WHY: LEB128 with MMKV sign-extension — negative values always take 10 bytes.
[[nodiscard]] size_t encodeInt32(int32_t value, std::vector<uint8_t>& out);
// WHY: LEB128 with MMKV sign-extension — negative values always take 10 bytes.
[[nodiscard]] size_t encodeInt64(int64_t value, std::vector<uint8_t>& out);
// Unsigned LEB128 varint (1..10 bytes).
[[nodiscard]] size_t encodeUInt64(uint64_t value, std::vector<uint8_t>& out);
// Raw IEEE-754, not varint. Always appends 4 bytes.
[[nodiscard]] size_t encodeFloat(float value, std::vector<uint8_t>& out);
// Raw IEEE-754, not varint. Always appends 8 bytes.
[[nodiscard]] size_t encodeDouble(double value, std::vector<uint8_t>& out);
// Appends the raw UTF-8 bytes of s with no length prefix. Returns s.size().
[[nodiscard]] size_t encodeString(std::string_view s, std::vector<uint8_t>& out);
// Appends [data, data+len) verbatim with no length prefix. Returns len.
[[nodiscard]] size_t encodeBytes(const uint8_t* data, size_t len, std::vector<uint8_t>& out);

} // namespace quickstore
