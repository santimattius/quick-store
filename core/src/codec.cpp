#include "quickstore/codec.h"
#include <cstring>
#include <cstdint>

namespace quickstore {

std::optional<bool> decodeBool(const uint8_t* blob, size_t len) {
    if (len != 1) return std::nullopt;
    if (blob[0] == 0x00) return false;
    if (blob[0] == 0x01) return true;
    return std::nullopt;
}

std::optional<int32_t> decodeInt32(const uint8_t* blob, size_t len) {
    if (len == 0) return std::nullopt;
    int32_t out = 0;
    bool error = false;
    size_t consumed = detail::readInt32(blob, blob + len, out, error);
    if (error || consumed != len) return std::nullopt;
    return out;
}

std::optional<int64_t> decodeInt64(const uint8_t* blob, size_t len) {
    if (len == 0) return std::nullopt;
    int64_t out = 0;
    bool error = false;
    size_t consumed = detail::readInt64(blob, blob + len, out, error);
    if (error || consumed != len) return std::nullopt;
    return out;
}

std::optional<uint32_t> decodeUInt32(const uint8_t* blob, size_t len) {
    if (len == 0) return std::nullopt;
    uint32_t out = 0;
    bool error = false;
    size_t consumed = detail::readUInt32(blob, blob + len, out, error);
    if (error || consumed != len) return std::nullopt;
    return out;
}

std::optional<uint64_t> decodeUInt64(const uint8_t* blob, size_t len) {
    if (len == 0) return std::nullopt;
    uint64_t out = 0;
    bool error = false;
    size_t consumed = detail::readUInt64(blob, blob + len, out, error);
    if (error || consumed != len) return std::nullopt;
    return out;
}

std::optional<float> decodeFloat(const uint8_t* blob, size_t len) {
    if (len != 4) return std::nullopt;
    float result;
    std::memcpy(&result, blob, 4);
    return result;
}

std::optional<double> decodeDouble(const uint8_t* blob, size_t len) {
    if (len != 8) return std::nullopt;
    double result;
    std::memcpy(&result, blob, 8);
    return result;
}

std::optional<std::string> decodeString(const uint8_t* blob, size_t len) {
    return std::string(reinterpret_cast<const char*>(blob), len);
}

std::optional<MMBuffer> decodeBytes(const uint8_t* blob, size_t len) {
    return MMBuffer(blob, len);
}


// --- Encode path (Fase 1) ---

size_t encodeBool(bool value, std::vector<uint8_t>& out) {
    out.push_back(value ? 0x01u : 0x00u);
    return 1;
}

size_t encodeInt32(int32_t value, std::vector<uint8_t>& out) {
    uint8_t buf[10];
    size_t n = detail::writeInt32(buf, value);
    out.insert(out.end(), buf, buf + n);
    return n;
}

size_t encodeInt64(int64_t value, std::vector<uint8_t>& out) {
    uint8_t buf[10];
    size_t n = detail::writeInt64(buf, value);
    out.insert(out.end(), buf, buf + n);
    return n;
}

size_t encodeUInt64(uint64_t value, std::vector<uint8_t>& out) {
    uint8_t buf[10];
    size_t n = detail::writeUInt64(buf, value);
    out.insert(out.end(), buf, buf + n);
    return n;
}

size_t encodeFloat(float value, std::vector<uint8_t>& out) {
    uint8_t tmp[4];
    std::memcpy(tmp, &value, 4);
    out.insert(out.end(), tmp, tmp + 4);
    return 4;
}

size_t encodeDouble(double value, std::vector<uint8_t>& out) {
    uint8_t tmp[8];
    std::memcpy(tmp, &value, 8);
    out.insert(out.end(), tmp, tmp + 8);
    return 8;
}

size_t encodeString(std::string_view s, std::vector<uint8_t>& out) {
    out.insert(out.end(),
               reinterpret_cast<const uint8_t*>(s.data()),
               reinterpret_cast<const uint8_t*>(s.data()) + s.size());
    return s.size();
}

size_t encodeBytes(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
    out.insert(out.end(), data, data + len);
    return len;
}

} // namespace quickstore
