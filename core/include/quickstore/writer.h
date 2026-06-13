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

/* Read-write handle over a QuickStore data file using MMKV's append-only log.
   Writes APPEND an encoded record; the same key written twice keeps both on disk
   and the latest one wins (last-write-wins via the in-memory index). When the
   mapping runs out of space it GROWS in place (ftruncate + re-mmap), doubling
   capacity. compact()/clearAll() rewrite the file with live entries only.
   Single-writer; optional multi-process mode guards writes with an exclusive
   file lock. Move-only. */
class QuickStoreWriter {
public:
    // Opens or creates "<rootDir>/<mmkvId>" (+ ".crc"). New files start at 4096 bytes. Existing files are CRC-validated and may roll back to the last confirmed state. itemSizeHolderSeed=0 picks a random seed. multiProcess=true enables flock-guarded writes. Returns nullopt on I/O failure, CRC failure, or lock acquisition failure.
    [[nodiscard]] static std::optional<QuickStoreWriter>
    open(const std::string& mmkvId, const std::string& rootDir,
         uint32_t itemSizeHolderSeed = 0, bool multiProcess = false);

    /* Encode value and APPEND a [key,value] record to the log; updates the index so
       later reads see this value (last-write-wins). Returns false if the store is
       closed or the underlying write/grow fails. Grows the mapping automatically when
       space runs out. */
    bool setBool(const std::string& key, bool value);
    bool setInt32(const std::string& key, int32_t value);
    bool setInt64(const std::string& key, int64_t value);
    bool setUInt64(const std::string& key, uint64_t value);
    bool setFloat(const std::string& key, float value);
    bool setDouble(const std::string& key, double value);
    bool setString(const std::string& key, std::string_view value);
    bool setBytes(const std::string& key, const uint8_t* data, size_t len);
    bool setBytes(const std::string& key, const std::vector<uint8_t>& data);
    // Appends an empty-value tombstone and drops the key from the index. Returns false if closed or the key is not present.
    // Does NOT erase bytes in-place; the record remains on disk until the next full write-back.
    bool remove(const std::string& key);
    // Drops all keys, then rewrites the file with an empty live set. Returns false if closed or the rewrite fails.
    bool clearAll();
    // Rewrites the file keeping only live entries, reclaiming space from overwritten/removed records. Returns false if closed or the rewrite fails.
    bool compact();

    /* Typed lookups served from the in-memory index (read-through). Return the value
       when the key exists and decodes as the requested type; nullopt otherwise. */
    [[nodiscard]] std::optional<bool>        getBool(const std::string& key) const;
    [[nodiscard]] std::optional<int32_t>     getInt32(const std::string& key) const;
    [[nodiscard]] std::optional<int64_t>     getInt64(const std::string& key) const;
    [[nodiscard]] std::optional<uint64_t>    getUInt64(const std::string& key) const;
    [[nodiscard]] std::optional<double>      getDouble(const std::string& key) const;
    [[nodiscard]] std::optional<std::string> getString(const std::string& key) const;
    [[nodiscard]] std::optional<MMBuffer>    getBytes(const std::string& key) const;

    // True if the key is live in the index.
    [[nodiscard]] bool                       contains(const std::string& key) const;
    // Snapshot of all live keys. Unordered.
    [[nodiscard]] std::vector<std::string>   allKeys() const;
    // Number of live keys.
    [[nodiscard]] size_t                     count() const;

    // Flushes state, releases the mapping and file lock, marks the handle closed. Idempotent; called by the destructor.
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
