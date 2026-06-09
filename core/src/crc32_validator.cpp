#include "quickstore/crc32_validator.h"
#include <zlib.h>

namespace quickstore {

uint32_t computeCRC(const uint8_t* base, uint32_t actualSize) {
    if (actualSize == 0) {
        return static_cast<uint32_t>(crc32(0L, nullptr, 0));
    }
    return static_cast<uint32_t>(crc32(0L, base + 4, actualSize));
}

bool validateCRC(const uint8_t* base, uint32_t actualSize, uint32_t expected) {
    return computeCRC(base, actualSize) == expected;
}


uint32_t updateCRC(uint32_t prev, const uint8_t* ptr, size_t len) noexcept {
    return static_cast<uint32_t>(::crc32(static_cast<uLong>(prev),
                                         reinterpret_cast<const Bytef*>(ptr),
                                         static_cast<uInt>(len)));
}

} // namespace quickstore
