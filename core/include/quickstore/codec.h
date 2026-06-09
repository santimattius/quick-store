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

[[nodiscard]] std::optional<bool>     decodeBool(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<int32_t>  decodeInt32(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<int64_t>  decodeInt64(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<uint32_t> decodeUInt32(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<uint64_t> decodeUInt64(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<float>    decodeFloat(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<double>   decodeDouble(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<std::string> decodeString(const uint8_t* blob, size_t len);
[[nodiscard]] std::optional<MMBuffer> decodeBytes(const uint8_t* blob, size_t len);

// --- Encode path (Fase 1) ---
// All functions APPEND to `out` (do not clear it) and return bytes appended.

[[nodiscard]] size_t encodeBool(bool value, std::vector<uint8_t>& out);
[[nodiscard]] size_t encodeInt32(int32_t value, std::vector<uint8_t>& out);
[[nodiscard]] size_t encodeInt64(int64_t value, std::vector<uint8_t>& out);
[[nodiscard]] size_t encodeUInt64(uint64_t value, std::vector<uint8_t>& out);
[[nodiscard]] size_t encodeFloat(float value, std::vector<uint8_t>& out);
[[nodiscard]] size_t encodeDouble(double value, std::vector<uint8_t>& out);
[[nodiscard]] size_t encodeString(std::string_view s, std::vector<uint8_t>& out);
[[nodiscard]] size_t encodeBytes(const uint8_t* data, size_t len, std::vector<uint8_t>& out);

} // namespace quickstore
