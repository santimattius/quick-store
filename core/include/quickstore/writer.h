#pragma once
#include "quickstore/codec.h"
#include "quickstore/crc32_validator.h"
#include "quickstore/data_parser.h"
#include "quickstore/file_lock.h"
#include "quickstore/key_value_holder.h"
#include "quickstore/meta_info.h"
#include "quickstore/mmap_region.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace quickstore {

class QuickStoreWriter {
public:
    [[nodiscard]] static std::optional<QuickStoreWriter>
    open(const std::string& mmkvId, const std::string& rootDir,
         uint32_t itemSizeHolderSeed = 0, bool multiProcess = false);

    bool setBool(const std::string& key, bool value);
    bool setInt32(const std::string& key, int32_t value);
    bool setInt64(const std::string& key, int64_t value);
    bool setUInt64(const std::string& key, uint64_t value);
    bool setFloat(const std::string& key, float value);
    bool setDouble(const std::string& key, double value);
    bool setString(const std::string& key, std::string_view value);
    bool setBytes(const std::string& key, const uint8_t* data, size_t len);
    bool setBytes(const std::string& key, const std::vector<uint8_t>& data);
    bool remove(const std::string& key);
    bool clearAll();
    bool compact();

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
    ~QuickStoreWriter();
    QuickStoreWriter(QuickStoreWriter&&) noexcept;
    QuickStoreWriter& operator=(QuickStoreWriter&&) noexcept;
    QuickStoreWriter(const QuickStoreWriter&)            = delete;
    QuickStoreWriter& operator=(const QuickStoreWriter&) = delete;

private:
    QuickStoreWriter(MmapRegion dataRegion, int crcFd,
                     std::unordered_map<std::string, KeyValueHolder> map,
                     MMKVMetaInfo meta, uint32_t seed) noexcept;

    bool appendRecord(const std::string& key, const std::vector<uint8_t>& valueBlob);
    bool ensureSpace(size_t needed);
    bool doFullWriteback();
    void updateMetaAfterAppend(const uint8_t* recordPtr, size_t recordSize);
    void checkLoadData() noexcept;
    void reloadFromFile() noexcept;

    MmapRegion                                       m_dataRegion;
    int                                              m_crcFd        = -1;
    std::unordered_map<std::string, KeyValueHolder>  m_map;
    MMKVMetaInfo                                     m_meta{};
    uint32_t                                         m_seed         = 0;
    bool                                             m_closed       = false;
    bool                                             m_multiProcess = false;
    std::unique_ptr<FileLock>                        m_fileLock;
    std::string                                      m_crcPath;
    std::string                                      m_dataPath;
};

} // namespace quickstore
