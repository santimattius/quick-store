#pragma once
#include <cstdint>

namespace quickstore {

struct KeyValueHolder {
    uint16_t computedKVSize;  // total record size (internal bookkeeping)
    uint16_t keySize;
    uint32_t valueSize;
    uint32_t offset;          // byte offset from base+4 to the start of this record
};

} // namespace quickstore
