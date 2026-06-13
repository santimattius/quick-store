#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace quickstore {

/* On-disk header stored in the companion ".crc" file (first 112 bytes).
   Mirrors MMKV's layout byte-for-byte so files stay interoperable. Fields are
   read/written at FIXED little-endian offsets (see static_asserts), independent
   of compiler struct padding. */
struct MMKVMetaInfo {
    uint32_t m_crcDigest;              // off 0:  CRC32 of the live data region [base+4, base+4+m_actualSize). Guards against corruption.
    uint32_t m_version;                // off 4:  Format version. Known values:
                                       //   1 = Sequence  (write-back counter added)
                                       //   2 = RandomIV  (AES IV stored in m_vector)
                                       //   3 = ActualSize (m_actualSize is authoritative; supersedes bytes [0,4))
                                       //   4 = Flag       (m_flags field active)
                                       // The writer emits 4; the reader handles 1–4.
    uint32_t m_sequence;               // off 8:  Full write-back counter; incremented on every doFullWriteBack, never on plain appends.
    uint8_t  m_vector[16];             // off 12: AES-CFB IV; random per full write-back. Active when m_version >= 2 (RandomIV).
    uint32_t m_actualSize;             // off 28: Authoritative data region size (bytes from offset 4). Active when m_version >= 3.
                                       // Supersedes the oldStyleActualSize stored at bytes [0,4) of the data file.
    uint32_t lastConfirmedActualSize;  // off 32: Last m_actualSize known to be CRC-consistent; used to roll back a torn/partial write.
    uint32_t lastConfirmedCRCDigest;   // off 36: CRC matching lastConfirmedActualSize; the known-good recovery point.
    uint8_t  _reserved[64];            // off 40: Reserved padding for MMKV layout compatibility; always zero, never serialized.
    uint64_t m_flags;                  // off 104: Feature flags. Bit 0 = EnableKeyExpire (per-key TTL active).
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
