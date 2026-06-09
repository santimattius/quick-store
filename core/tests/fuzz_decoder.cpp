// fuzz_decoder.cpp
// libFuzzer harness for the quickstore decoder layer.
//
// Entry point: LLVMFuzzerTestOneInput
//   - Treats the entire input as a candidate data file.
//   - Parses the first 112 bytes as a MMKVMetaInfo via parseMetaInfo.
//   - On valid meta: calls checkDataValid + decodeMap (both Discard and greedy).
//   - Returns 0 on every path — never crashes, never aborts.
//
// Build (Clang only):
//   cmake -DQUICKSTORE_BUILD_TESTS=ON -B build_fuzz \
//         -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
//   cmake --build build_fuzz --target fuzz_decoder
//
// Run:
//   ./build_fuzz/core/tests/fuzz_decoder -runs=1000

#include "quickstore/meta_info.h"
#include "quickstore/data_validator.h"
#include "quickstore/data_parser.h"
#include <cstdint>
#include <cstddef>
#include <algorithm>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // parseMetaInfo requires at least 112 bytes to deserialize a valid header.
    if (size < 4) {
        return 0;
    }

    // Attempt to parse the first 112 bytes as MMKVMetaInfo.
    // Returns nullopt if size < 112 — handled gracefully below.
    auto meta_opt = quickstore::parseMetaInfo(data, size);
    if (!meta_opt) {
        // Input too short to hold a full meta block — nothing to fuzz.
        return 0;
    }

    const quickstore::MMKVMetaInfo& meta = *meta_opt;

    // Clamp actualSize to the actual input size so decodeMap never reads
    // beyond the fuzzer-provided buffer (avoids OOB on crafted large sizes).
    uint32_t actual_size = std::min(meta.m_actualSize, static_cast<uint32_t>(size));

    // Call checkDataValid — exercises the CRC validation and lastConfirmed
    // fallback paths. RecoverStrategy::Discard is the production default.
    quickstore::checkDataValid(data, size, meta, quickstore::RecoverStrategy::Discard);

    // Call decodeMap on both paths:
    //   greedy=false — stops on first malformed record (production default)
    //   greedy=true  — skips corrupt bytes and continues best-effort
    quickstore::decodeMap(data, actual_size, false);
    quickstore::decodeMap(data, actual_size, true);

    return 0;
}
