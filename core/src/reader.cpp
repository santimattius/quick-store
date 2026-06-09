#include "quickstore/reader.h"
#include "value_blob.h"

namespace quickstore {

std::optional<QuickStoreReader> QuickStoreReader::open(const std::string& mmap_id,
                                                       const std::string& root_dir) {
    std::string data_path = root_dir + "/" + mmap_id;
    std::string crc_path  = data_path + ".crc";

    auto region = MmapRegion::open(data_path, AccessMode::ReadOnly);
    if (!region) return std::nullopt;

    auto meta = readMetaInfo(crc_path);
    if (!meta) return std::nullopt;

    ValidateResult r = checkDataValid(region->base(), region->size(),
                                      *meta, RecoverStrategy::Discard);
    if (!r.loadFromFile) return std::nullopt;

    auto map = decodeMap(region->base(), r.effectiveActualSize,
                         /*greedy=*/ r.needFullWriteback);

    return QuickStoreReader(std::move(*region), std::move(map));
}


std::optional<bool> QuickStoreReader::getBool(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(regionBase(), regionSize(), it->second);
    if (!ptr) return std::nullopt;
    return decodeBool(ptr, len);
}

std::optional<int32_t> QuickStoreReader::getInt32(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(regionBase(), regionSize(), it->second);
    if (!ptr) return std::nullopt;
    return decodeInt32(ptr, len);
}

std::optional<int64_t> QuickStoreReader::getInt64(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(regionBase(), regionSize(), it->second);
    if (!ptr) return std::nullopt;
    return decodeInt64(ptr, len);
}

std::optional<uint64_t> QuickStoreReader::getUInt64(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(regionBase(), regionSize(), it->second);
    if (!ptr) return std::nullopt;
    return decodeUInt64(ptr, len);
}

std::optional<double> QuickStoreReader::getDouble(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(regionBase(), regionSize(), it->second);
    if (!ptr) return std::nullopt;
    return decodeDouble(ptr, len);
}

std::optional<std::string> QuickStoreReader::getString(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(regionBase(), regionSize(), it->second);
    if (!ptr) return std::nullopt;
    return decodeString(ptr, len);
}

std::optional<MMBuffer> QuickStoreReader::getBytes(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(regionBase(), regionSize(), it->second);
    if (!ptr) return std::nullopt;
    return decodeBytes(ptr, len);
}

bool QuickStoreReader::contains(const std::string& key) const {
    return m_map.find(key) != m_map.end();
}

std::vector<std::string> QuickStoreReader::allKeys() const {
    std::vector<std::string> keys;
    keys.reserve(m_map.size());
    for (const auto& kv : m_map) {
        keys.push_back(kv.first);
    }
    return keys;
}

size_t QuickStoreReader::count() const {
    return m_map.size();
}

void QuickStoreReader::close() {
    if (!m_closed) {
        m_closed = true;
        m_map.clear();
        // Reset the optional to trigger MmapRegion destructor (munmap + fd close).
        m_region.reset();
    }
}

} // namespace quickstore
