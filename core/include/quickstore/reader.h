#pragma once
#include "quickstore/codec.h"
#include "quickstore/data_parser.h"
#include "quickstore/data_validator.h"
#include "quickstore/key_value_holder.h"
#include "quickstore/meta_info.h"
#include "quickstore/mmap_region.h"
#include "quickstore/mmbuffer.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace quickstore {

/* Read-only view over a QuickStore data file. Maps the file private/read-only,
   validates its CRC against the ".crc" header at open() time, and builds an
   in-memory key -> value-location index for O(1) typed lookups. Immutable:
   never writes to disk.
   Not copyable; move-only. The mmap region is exclusively owned. */
class QuickStoreReader {
public:
    // Opens "<root_dir>/<mmap_id>" + its ".crc". Returns nullopt if either file is missing/empty, the header is shorter than 112 bytes, or the CRC check fails.
    [[nodiscard]] static std::optional<QuickStoreReader>
    open(const std::string& mmap_id, const std::string& root_dir);

    /* Typed lookups. Return the value if the key exists AND the stored bytes
       decode as the requested type; nullopt if the key is absent or the bytes
       are malformed for that type. Reading a value as the wrong type yields nullopt.
       Scalar getters (bool, long, double) do not heap-allocate; getBytes copies into a new MMBuffer. */
    [[nodiscard]] std::optional<bool>        getBool(const std::string& key) const;
    [[nodiscard]] std::optional<int32_t>     getInt32(const std::string& key) const;
    [[nodiscard]] std::optional<int64_t>     getInt64(const std::string& key) const;
    [[nodiscard]] std::optional<uint64_t>    getUInt64(const std::string& key) const;
    [[nodiscard]] std::optional<double>      getDouble(const std::string& key) const;
    [[nodiscard]] std::optional<std::string> getString(const std::string& key) const;
    [[nodiscard]] std::optional<MMBuffer>    getBytes(const std::string& key) const;

    // True if the key exists in the index, regardless of value type.
    [[nodiscard]] bool                       contains(const std::string& key) const;
    // Snapshot of all live keys (tombstoned keys excluded). Unordered.
    [[nodiscard]] std::vector<std::string>   allKeys() const;
    // Number of live keys.
    [[nodiscard]] size_t                     count() const;

    // Unmaps the region and clears the index; subsequent reads return nullopt/empty. Idempotent.
    void close();

private:
    QuickStoreReader(MmapRegion region,
                     std::unordered_map<std::string, KeyValueHolder> map)
        : m_region(std::move(region))
        , m_map(std::move(map))
        , m_closed(false) {}

    const uint8_t* regionBase() const {
        return m_region ? m_region->base() : nullptr;
    }
    size_t regionSize() const {
        return m_region ? m_region->size() : 0;
    }

    std::optional<MmapRegion>                       m_region;
    std::unordered_map<std::string, KeyValueHolder> m_map;
    bool                                            m_closed;
};

} // namespace quickstore
