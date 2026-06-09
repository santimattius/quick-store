#include "quickstore/data_validator.h"

namespace quickstore {

ValidateResult checkDataValid(const uint8_t* base, size_t fileSize,
                              const MMKVMetaInfo& meta,
                              RecoverStrategy strat) {
    uint32_t candidateActualSize = 0;
    if (meta.m_version < 3) {
        candidateActualSize = readOldStyleActualSize(base);
    } else {
        candidateActualSize = meta.m_actualSize;
    }

    // Primary path: truncation check + CRC.
    if (static_cast<uint64_t>(candidateActualSize) + 4 <= static_cast<uint64_t>(fileSize)
        && validateCRC(base, candidateActualSize, meta.m_crcDigest)) {
        return {true, false, candidateActualSize};
    }

    // lastConfirmed path (requires m_version >= 2).
    if (meta.m_version >= 2) {
        uint32_t lastActualSize = meta.lastConfirmedActualSize;
        if (static_cast<uint64_t>(lastActualSize) + 4 <= static_cast<uint64_t>(fileSize)
            && validateCRC(base, lastActualSize, meta.lastConfirmedCRCDigest)) {
            return {true, true, lastActualSize};
        }
    }

    // Recovery strategy fallback.
    if (strat == RecoverStrategy::Recover) {
        return {true, true, candidateActualSize};
    }
    return {false, false, 0};
}

} // namespace quickstore
