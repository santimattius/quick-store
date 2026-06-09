#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace quickstore {

struct MMKVMetaInfo {
    uint32_t m_crcDigest;
    uint32_t m_version;
    uint32_t m_sequence;
    uint8_t  m_vector[16];
    uint32_t m_actualSize;
    uint32_t lastConfirmedActualSize;
    uint32_t lastConfirmedCRCDigest;
    uint8_t  _reserved[64];
    uint64_t m_flags;
};

static_assert(sizeof(MMKVMetaInfo) == 112,                            "MMKVMetaInfo size mismatch");
static_assert(offsetof(MMKVMetaInfo, m_crcDigest)             == 0,   "off m_crcDigest");
static_assert(offsetof(MMKVMetaInfo, m_version)               == 4,   "off m_version");
static_assert(offsetof(MMKVMetaInfo, m_sequence)              == 8,   "off m_sequence");
static_assert(offsetof(MMKVMetaInfo, m_vector)                == 12,  "off m_vector");
static_assert(offsetof(MMKVMetaInfo, m_actualSize)            == 28,  "off m_actualSize");
static_assert(offsetof(MMKVMetaInfo, lastConfirmedActualSize) == 32,  "off lastConfirmedActualSize");
static_assert(offsetof(MMKVMetaInfo, lastConfirmedCRCDigest)  == 36,  "off lastConfirmedCRCDigest");
static_assert(offsetof(MMKVMetaInfo, _reserved)               == 40,  "off _reserved");
static_assert(offsetof(MMKVMetaInfo, m_flags)                 == 104, "off m_flags");

constexpr size_t kMetaInfoSize = 112;

// Explicit deserialization from a buffer at FIXED offsets (little-endian).
// Independent of compiler struct layout. _reserved stays zero-initialized (not copied).
// Returns nullopt if len < 112.
[[nodiscard]] std::optional<MMKVMetaInfo> parseMetaInfo(const uint8_t* buf, size_t len);

// Reads the .crc file, reads first 112 bytes, calls parseMetaInfo.
// Returns nullopt if file missing, unreadable, or size < 112 bytes.
[[nodiscard]] std::optional<MMKVMetaInfo> readMetaInfo(const std::string& crc_path);

// --- Write path (Fase 1) ---

// Serializes all fields of `info` into a zero-initialized 112-byte buffer at exact
// offsets (mirror of parseMetaInfo) and writes it via pwrite(fd, buf, 112, 0).
// Returns true iff pwrite wrote all 112 bytes.
// WHY: uses pwrite (stateless, no lseek) so the cursor of m_crcFd stays unaffected.
[[nodiscard]] bool writeMetaInfo(int fd, const MMKVMetaInfo& info) noexcept;

// Fast path: updates only crcDigest (bytes [0,4)) and actualSize (bytes [28,32))
// in the .crc file via two pwrite calls. All other bytes are unchanged.
// WHY: on every append only these two fields change; writing 8 bytes instead of 112
// is the MMKV hot-path optimization.
void writeMetaInfoFast(int fd, uint32_t crcDigest, uint32_t actualSize) noexcept;

} // namespace quickstore
