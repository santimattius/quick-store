#include "quickstore/varint.h"

namespace quickstore::detail {

size_t readUInt32(const uint8_t* p, const uint8_t* end,
                  uint32_t& out, bool& error) {
    if (p >= end) {
        error = true;
        return 0;
    }
    uint32_t result = 0;
    int shift = 0;
    const uint8_t* cur = p;
    while (cur < end) {
        uint8_t byte = *cur++;
        result |= static_cast<uint32_t>(byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) {
            out = result;
            return static_cast<size_t>(cur - p);
        }
        if (shift >= 35) {
            // More than 5 bytes for uint32 is an overflow.
            error = true;
            return 0;
        }
    }
    // Ran out of bytes before terminal byte.
    error = true;
    return 0;
}

size_t readUInt64(const uint8_t* p, const uint8_t* end,
                  uint64_t& out, bool& error) {
    if (p >= end) {
        error = true;
        return 0;
    }
    uint64_t result = 0;
    int shift = 0;
    const uint8_t* cur = p;
    while (cur < end) {
        uint8_t byte = *cur++;
        result |= static_cast<uint64_t>(byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) {
            out = result;
            return static_cast<size_t>(cur - p);
        }
        if (shift >= 70) {
            // More than 10 bytes for uint64 is an overflow.
            error = true;
            return 0;
        }
    }
    error = true;
    return 0;
}

size_t readInt32(const uint8_t* p, const uint8_t* end,
                 int32_t& out, bool& error) {
    // WHY: MMKV sign-extends negative int32 to full 64-bit before encoding.
    // Reading as uint64 (up to 10 bytes) and static_cast<int32_t> reproduces the
    // original value. A bare uint32 read would truncate and lose negative encoding.
    uint64_t tmp = 0;
    size_t n = readUInt64(p, end, tmp, error);
    if (!error) {
        out = static_cast<int32_t>(tmp);
    }
    return n;
}

size_t readInt64(const uint8_t* p, const uint8_t* end,
                 int64_t& out, bool& error) {
    uint64_t tmp = 0;
    size_t n = readUInt64(p, end, tmp, error);
    if (!error) {
        out = static_cast<int64_t>(tmp);
    }
    return n;
}


size_t writeUInt32(uint8_t* dst, uint32_t v) noexcept {
    size_t n = 0;
    while (v >= 0x80u) {
        dst[n++] = static_cast<uint8_t>((v & 0x7Fu) | 0x80u);
        v >>= 7;
    }
    dst[n++] = static_cast<uint8_t>(v);
    return n;
}

size_t writeUInt64(uint8_t* dst, uint64_t v) noexcept {
    size_t n = 0;
    while (v >= 0x80u) {
        dst[n++] = static_cast<uint8_t>((v & 0x7Fu) | 0x80u);
        v >>= 7;
    }
    dst[n++] = static_cast<uint8_t>(v);
    return n;
}

size_t writeInt32(uint8_t* dst, int32_t v) noexcept {
    // WHY: mirrors readInt32 — sign-extend to int64 then cast to uint64.
    // Negative values become 0xFFFFFFFF... which encodes to 10 bytes in LEB128.
    return writeUInt64(dst, static_cast<uint64_t>(static_cast<int64_t>(v)));
}

size_t writeInt64(uint8_t* dst, int64_t v) noexcept {
    // WHY: mirrors readInt64 — cast to uint64, negative fills upper bits → 10 bytes.
    return writeUInt64(dst, static_cast<uint64_t>(v));
}

size_t varIntSize(uint64_t v) noexcept {
    size_t n = 0;
    do {
        ++n;
        v >>= 7;
    } while (v != 0);
    return n;
}

} // namespace quickstore::detail
