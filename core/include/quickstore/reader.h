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

class QuickStoreReader {
public:
    [[nodiscard]] static std::optional<QuickStoreReader>
    open(const std::string& mmap_id, const std::string& root_dir);

    [[nodiscard]] std::optional<bool>        getBool(const std::string& key) const;
    [[nodiscard]] std::optional<int32_t>     getInt32(const std::string& key) const;
    [[nodiscard]] std::optional<int64_t>     getInt64(const std::string& key) const;
    [[nodiscard]] std::optional<uint64_t>    getUInt64(const std::string& key) const;
    [[nodiscard]] std::optional<double>      getDouble(const std::string& key) const;
    [[nodiscard]] std::optional<std::string> getString(const std::string& key) const;
    [[nodiscard]] std::optional<MMBuffer>    getBytes(const std::string& key) const;

    [[nodiscard]] bool                       contains(const std::string& key) const;
    [[nodiscard]] std::vector<std::string>   allKeys() const;
    [[nodiscard]] size_t                     count() const;

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
