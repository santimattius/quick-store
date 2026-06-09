#include "quickstore/meta_info.h"
#include <cstdio>
#include <unistd.h>

namespace quickstore {

std::optional<MMKVMetaInfo> parseMetaInfo(const uint8_t* buf, size_t len) {
    if (len < kMetaInfoSize) return std::nullopt;
    MMKVMetaInfo info{};
    std::memcpy(&info.m_crcDigest,             buf + 0,   4);
    std::memcpy(&info.m_version,               buf + 4,   4);
    std::memcpy(&info.m_sequence,              buf + 8,   4);
    std::memcpy(&info.m_vector,                buf + 12, 16);
    std::memcpy(&info.m_actualSize,            buf + 28,  4);
    std::memcpy(&info.lastConfirmedActualSize, buf + 32,  4);
    std::memcpy(&info.lastConfirmedCRCDigest,  buf + 36,  4);
    // WHY: bytes [40, 104) = _reserved — intentionally NOT copied; stays zero-initialized.
    std::memcpy(&info.m_flags,                 buf + 104, 8);
    return info;
}

std::optional<MMKVMetaInfo> readMetaInfo(const std::string& crc_path) {
    std::FILE* f = std::fopen(crc_path.c_str(), "rb");
    if (!f) return std::nullopt;

    uint8_t buf[kMetaInfoSize];
    size_t read = std::fread(buf, 1, kMetaInfoSize, f);
    std::fclose(f);

    if (read < kMetaInfoSize) return std::nullopt;
    return parseMetaInfo(buf, kMetaInfoSize); // always has_value() here — len == kMetaInfoSize
}


bool writeMetaInfo(int fd, const MMKVMetaInfo& info) noexcept {
    uint8_t buf[kMetaInfoSize]{};  // zero-initialized — _reserved bytes stay 0
    std::memcpy(buf + 0,   &info.m_crcDigest,             4);
    std::memcpy(buf + 4,   &info.m_version,               4);
    std::memcpy(buf + 8,   &info.m_sequence,              4);
    std::memcpy(buf + 12,  &info.m_vector,               16);
    std::memcpy(buf + 28,  &info.m_actualSize,            4);
    std::memcpy(buf + 32,  &info.lastConfirmedActualSize, 4);
    std::memcpy(buf + 36,  &info.lastConfirmedCRCDigest,  4);
    // bytes [40, 104) = _reserved — already zero from {} init
    std::memcpy(buf + 104, &info.m_flags,                 8);
    ssize_t n = ::pwrite(fd, buf, kMetaInfoSize, 0);
    return n == static_cast<ssize_t>(kMetaInfoSize);
}

void writeMetaInfoFast(int fd, uint32_t crcDigest, uint32_t actualSize) noexcept {
    ::pwrite(fd, &crcDigest,  4, 0);   // bytes [0,4)
    ::pwrite(fd, &actualSize, 4, 28);  // bytes [28,32)
}

} // namespace quickstore
