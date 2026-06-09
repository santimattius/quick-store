#pragma once
#include "quickstore/crc32_validator.h"
#include "quickstore/data_parser.h"
#include "quickstore/meta_info.h"
#include <cstddef>
#include <cstdint>

namespace quickstore {

struct ValidateResult {
    bool     loadFromFile;
    bool     needFullWriteback;
    uint32_t effectiveActualSize;
};

enum class RecoverStrategy { Discard, Recover };

// Implements the checkDataValid decision tree (spec §6.6).
// Pure function: no I/O, no side effects.
[[nodiscard]] ValidateResult checkDataValid(const uint8_t* base, size_t fileSize,
                                            const MMKVMetaInfo& meta,
                                            RecoverStrategy strat);

} // namespace quickstore
