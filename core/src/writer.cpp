#include "quickstore/writer.h"
#include "quickstore/data_validator.h"
#include "quickstore/varint.h"
#include "value_blob.h"
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace quickstore {

// ============================================================
// Private constructor / destructor / move
// ============================================================

QuickStoreWriter::QuickStoreWriter(MmapRegion dataRegion, int crcFd,
        std::unordered_map<std::string, KeyValueHolder> map,
        MMKVMetaInfo meta, uint32_t seed) noexcept
    : m_dataRegion(std::move(dataRegion))
    , m_crcFd(crcFd)
    , m_map(std::move(map))
    , m_meta(meta)
    , m_seed(seed)
    , m_closed(false) {}

QuickStoreWriter::~QuickStoreWriter() { close(); }

QuickStoreWriter::QuickStoreWriter(QuickStoreWriter&& o) noexcept
    : m_dataRegion(std::move(o.m_dataRegion))
    , m_crcFd(o.m_crcFd)
    , m_map(std::move(o.m_map))
    , m_meta(o.m_meta)
    , m_seed(o.m_seed)
    , m_closed(o.m_closed)
    , m_multiProcess(o.m_multiProcess)
    , m_fileLock(std::move(o.m_fileLock))
    , m_crcPath(std::move(o.m_crcPath))
    , m_dataPath(std::move(o.m_dataPath)) {
    o.m_crcFd        = -1;
    o.m_closed       = true;
    o.m_multiProcess = false;
}

QuickStoreWriter& QuickStoreWriter::operator=(QuickStoreWriter&& o) noexcept {
    if (this != &o) {
        close();
        m_dataRegion   = std::move(o.m_dataRegion);
        m_crcFd        = o.m_crcFd;
        m_map          = std::move(o.m_map);
        m_meta         = o.m_meta;
        m_seed         = o.m_seed;
        m_closed       = o.m_closed;
        m_multiProcess = o.m_multiProcess;
        m_fileLock     = std::move(o.m_fileLock);
        m_crcPath      = std::move(o.m_crcPath);
        m_dataPath     = std::move(o.m_dataPath);
        o.m_crcFd        = -1;
        o.m_closed       = true;
        o.m_multiProcess = false;
    }
    return *this;
}

void QuickStoreWriter::close() {
    if (!m_closed) {
        m_closed = true;
        m_map.clear();
        if (m_crcFd != -1) {
            ::close(m_crcFd);
            m_crcFd = -1;
        }
        // MmapRegion destructor handles munmap + fd close
    }
}

// ============================================================
// open() — T31 (new file) + T32 (existing file)
// ============================================================

std::optional<QuickStoreWriter> QuickStoreWriter::open(
        const std::string& mmkvId,
        const std::string& rootDir,
        uint32_t seed,
        bool multiProcess) {

    std::string dataPath = rootDir + "/" + mmkvId;
    std::string crcPath  = dataPath + ".crc";

    struct stat st{};
    bool exists = (::stat(dataPath.c_str(), &st) == 0);

    if (!exists) {
        // --- NEW FILE (T31) ---

        // Create and truncate to 4096 bytes
        int fd = ::open(dataPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (fd < 0) return std::nullopt;
        if (::ftruncate(fd, 4096) != 0) { ::close(fd); return std::nullopt; }
        ::close(fd); // MmapRegion will open its own fd

        auto region = MmapRegion::open(dataPath, AccessMode::ReadWrite);
        if (!region) return std::nullopt;

        if (seed == 0) {
            ::arc4random_buf(&seed, sizeof(seed));
        }

        uint8_t* base = region->mutableBase();

        // actualSize = 4 (just the ItemSizeHolder occupies the counted region)
        uint32_t initActual = 4;

        // Write oldStyleActualSize at base[0,4)
        std::memcpy(base + 0, &initActual, 4);

        // Write ItemSizeHolder (seed) at base+4
        std::memcpy(base + 4, &seed, 4);

        // Compute CRC over [base+4, base+4+actualSize) = [base+4, base+8)
        uint32_t initCRC = computeCRC(base, initActual);

        MMKVMetaInfo meta{};
        meta.m_crcDigest  = initCRC;
        meta.m_version    = 4;
        meta.m_sequence   = 0;
        meta.m_actualSize = initActual;
        meta.lastConfirmedActualSize = initActual;
        meta.lastConfirmedCRCDigest  = initCRC;

        int crcFd = ::open(crcPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (crcFd < 0) return std::nullopt;
        if (::ftruncate(crcFd, 112) != 0) { ::close(crcFd); return std::nullopt; }
        writeMetaInfo(crcFd, meta);

        QuickStoreWriter w(std::move(*region), crcFd, {}, meta, seed);
        w.m_crcPath  = crcPath;
        w.m_dataPath = dataPath;
        if (multiProcess) {
            w.m_multiProcess = true;
            w.m_fileLock = std::make_unique<FileLock>(w.m_crcFd);
            if (!w.m_fileLock->lock(LockType::ExclusiveLock)) return std::nullopt;
            w.m_fileLock->unlock(LockType::ExclusiveLock);
        }
        return w;

    } else {
        // --- EXISTING FILE (T32) ---

        auto region = MmapRegion::open(dataPath, AccessMode::ReadWrite);
        if (!region) return std::nullopt;

        auto meta = readMetaInfo(crcPath);
        if (!meta) return std::nullopt;

        ValidateResult r = checkDataValid(region->base(), region->size(),
                                          *meta, RecoverStrategy::Discard);
        if (!r.loadFromFile) return std::nullopt;

        auto map = decodeMap(region->base(), r.effectiveActualSize, r.needFullWriteback);

        int crcFd = ::open(crcPath.c_str(), O_RDWR | O_CLOEXEC);
        if (crcFd < 0) return std::nullopt;

        // Recover the ItemSizeHolder seed from the existing file at base+4
        uint32_t usedSeed = 0;
        if (region->size() >= 8) {
            std::memcpy(&usedSeed, region->base() + 4, 4);
        }

        // If validation forced a different actualSize (lastConfirmed path), sync meta
        meta->m_actualSize = r.effectiveActualSize;

        QuickStoreWriter w(std::move(*region), crcFd, std::move(map), *meta, usedSeed);
        w.m_crcPath  = crcPath;
        w.m_dataPath = dataPath;
        if (multiProcess) {
            w.m_multiProcess = true;
            w.m_fileLock = std::make_unique<FileLock>(w.m_crcFd);
            if (!w.m_fileLock->lock(LockType::ExclusiveLock)) return std::nullopt;
            w.m_fileLock->unlock(LockType::ExclusiveLock);
        }
        return w;
    }
}

// ============================================================
// ensureSpace — T34
// ============================================================

bool QuickStoreWriter::ensureSpace(size_t needed) {
    // Data layout:
    //   base[0,4)     : oldStyleActualSize (4 bytes, not in m_actualSize region)
    //   base[4, 4+m_actualSize) : CRC'd region (ItemSizeHolder + records)
    //
    // Available capacity for the data region: capacity() - 4
    // Used by data: m_actualSize
    // Free: capacity() - 4 - m_actualSize

    size_t capacity = m_dataRegion.capacity();
    size_t dataCapacity = (capacity > 4) ? (capacity - 4) : 0;
    size_t freeSpace    = (dataCapacity > m_meta.m_actualSize)
                        ? (dataCapacity - m_meta.m_actualSize)
                        : 0;

    if (freeSpace >= needed) return true;

    size_t newCap = capacity;
    if (newCap == 0) newCap = 4096;
    while (newCap - 4 < m_meta.m_actualSize + needed) {
        newCap *= 2;
    }

    return m_dataRegion.grow(newCap);
    // Caller MUST re-fetch mutableBase() after this call
}

// ============================================================
// updateMetaAfterAppend — helper
// ============================================================

void QuickStoreWriter::updateMetaAfterAppend(const uint8_t* recordPtr, size_t recordSize) {
    // Update rolling CRC BEFORE incrementing actualSize (we pass the record bytes)
    m_meta.m_crcDigest  = updateCRC(m_meta.m_crcDigest, recordPtr, recordSize);
    m_meta.m_actualSize += static_cast<uint32_t>(recordSize);

    // Sync oldStyleActualSize at base[0,4)
    uint8_t* base = m_dataRegion.mutableBase();
    std::memcpy(base, &m_meta.m_actualSize, 4);

    if (m_multiProcess) {
        // C1 fix: bump sequence so peers can detect this write via checkLoadData()
        m_meta.m_sequence++;
        writeMetaInfo(m_crcFd, m_meta);  // full 112-byte write (persists seq@8 + actualSize@28 + crc@0)
    } else {
        writeMetaInfoFast(m_crcFd, m_meta.m_crcDigest, m_meta.m_actualSize);
    }
}

// ============================================================
// checkLoadData / reloadFromFile — T87 (multi-process change detection)
// ============================================================

void QuickStoreWriter::checkLoadData() noexcept {
    if (m_crcFd < 0) return;

    // m_sequence @ offset 8, m_actualSize @ offset 28 (verified by static_assert in meta_info.h)
    uint32_t diskSeq    = 0;
    uint32_t diskActual = 0;
    if (::pread(m_crcFd, &diskSeq,    4,  8) != 4) return;
    if (::pread(m_crcFd, &diskActual, 4, 28) != 4) return;

    if (diskSeq == m_meta.m_sequence && diskActual == m_meta.m_actualSize) return;
    reloadFromFile();
}

void QuickStoreWriter::reloadFromFile() noexcept {
    auto newMeta = quickstore::readMetaInfo(m_crcPath);
    if (!newMeta) return;
    // data layout: base[0,4)=oldStyleActualSize, base[4, 4+actualSize)=CRC'd region
    // Use MmapRegion::open to remap the data file at its CURRENT size (as set by the peer).
    // Do NOT call grow() here — grow() calls ftruncate which could shrink a file
    // that another process has already expanded.
    size_t neededSize = 4 + static_cast<size_t>(newMeta->m_actualSize);
    if (neededSize > m_dataRegion.size()) {
        // Re-open the data region to pick up the new file size set by the peer.
        auto newRegion = MmapRegion::open(m_dataPath, AccessMode::ReadWrite);
        if (!newRegion) return;
        m_dataRegion = std::move(*newRegion);
    }

    m_meta = *newMeta;

    // Re-fetch base AFTER possible remap
    const uint8_t* base = m_dataRegion.base();
    m_map = decodeMap(base, m_meta.m_actualSize, false);
}

// ============================================================
// appendRecord — T33
// ============================================================

bool QuickStoreWriter::appendRecord(const std::string& key,
                                    const std::vector<uint8_t>& valueBlob) {
    // Multi-process: acquire exclusive lock and reload if a peer wrote since last op.
    // RAII guard ensures the lock is released on every return path (including early returns).
    struct ExclusiveLockGuard {
        FileLock* lock;
        bool      active;
        ~ExclusiveLockGuard() noexcept { if (active && lock) lock->unlock(LockType::ExclusiveLock); }
    };
    ExclusiveLockGuard lockGuard{nullptr, false};

    if (m_multiProcess && m_fileLock) {
        if (!m_fileLock->lock(LockType::ExclusiveLock)) return false;
        lockGuard.lock   = m_fileLock.get();
        lockGuard.active = true;
        checkLoadData();
    }

    // Build encoded record: [varint(keyLen)][key bytes][varint(valueLen)][value bytes]
    uint8_t tmp[10];
    std::vector<uint8_t> record;
    record.reserve(10 + key.size() + 10 + valueBlob.size());

    size_t n = detail::writeUInt32(tmp, static_cast<uint32_t>(key.size()));
    record.insert(record.end(), tmp, tmp + n);
    record.insert(record.end(),
                  reinterpret_cast<const uint8_t*>(key.data()),
                  reinterpret_cast<const uint8_t*>(key.data()) + key.size());

    n = detail::writeUInt32(tmp, static_cast<uint32_t>(valueBlob.size()));
    record.insert(record.end(), tmp, tmp + n);
    record.insert(record.end(), valueBlob.begin(), valueBlob.end());

    if (!ensureSpace(record.size())) return false;

    // Re-fetch base AFTER ensureSpace (grow may have re-mmaped)
    uint8_t* base = m_dataRegion.mutableBase();

    // Save offset BEFORE updating actualSize
    uint32_t recordOffset = m_meta.m_actualSize;  // offset from base+4

    // Write record at base + 4 + m_actualSize
    uint8_t* dst = base + 4 + m_meta.m_actualSize;
    std::memcpy(dst, record.data(), record.size());

    // Update meta (also syncs oldStyleActualSize + writeMetaInfo/writeMetaInfoFast)
    updateMetaAfterAppend(record.data(), record.size());

    // Update m_map with holder pointing to this record
    uint32_t valueSize = static_cast<uint32_t>(valueBlob.size());
    if (valueSize == 0) {
        m_map.erase(key);  // tombstone
    } else {
        KeyValueHolder h{};
        h.offset          = recordOffset;
        h.keySize         = static_cast<uint16_t>(key.size());
        h.valueSize       = valueSize;
        uint32_t kvSize   = static_cast<uint32_t>(record.size());
        h.computedKVSize  = static_cast<uint16_t>(kvSize > 0xFFFF ? 0xFFFF : kvSize);
        m_map[key] = h;
    }
    return true;
    // lockGuard destructor releases ExclusiveLock here
}

// ============================================================
// doFullWriteback — T35
// ============================================================

bool QuickStoreWriter::doFullWriteback() {
    // Multi-process: acquire exclusive lock to prevent peers from reading torn state.
    // RAII guard ensures release on every return path.
    struct ExclusiveLockGuard {
        FileLock* lock;
        bool      active;
        ~ExclusiveLockGuard() noexcept { if (active && lock) lock->unlock(LockType::ExclusiveLock); }
    };
    ExclusiveLockGuard lockGuard{nullptr, false};

    if (m_multiProcess && m_fileLock) {
        if (!m_fileLock->lock(LockType::ExclusiveLock)) return false;
        lockGuard.lock   = m_fileLock.get();
        lockGuard.active = true;
        checkLoadData();  // adopt peer state before rewriting, else we drop their keys
    }

    // Serialize all live entries from m_map
    uint8_t tmp[10];
    std::vector<uint8_t> preparedData;

    for (const auto& [key, h] : m_map) {
        // Read the existing value blob from current mapping
        auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), h);
        if (!ptr || len == 0) continue;  // skip tombstones (shouldn't be in m_map)

        // Encode key
        size_t n = detail::writeUInt32(tmp, static_cast<uint32_t>(key.size()));
        preparedData.insert(preparedData.end(), tmp, tmp + n);
        preparedData.insert(preparedData.end(),
                            reinterpret_cast<const uint8_t*>(key.data()),
                            reinterpret_cast<const uint8_t*>(key.data()) + key.size());

        // Encode value
        n = detail::writeUInt32(tmp, len);
        preparedData.insert(preparedData.end(), tmp, tmp + n);
        preparedData.insert(preparedData.end(), ptr, ptr + len);
    }

    // newActualSize = 4 (ItemSizeHolder) + preparedData.size()
    uint32_t newActualSize = 4 + static_cast<uint32_t>(preparedData.size());

    // Ensure we have enough space for the new layout
    if (!ensureSpace(newActualSize)) return false;

    // Regenerate ItemSizeHolder seed
    uint32_t newSeed = m_seed;
    if (newSeed == 0) {
        ::arc4random_buf(&newSeed, sizeof(newSeed));
    } else {
        // For deterministic tests: increment to make it differ from previous value
        // In production this would be random
        ::arc4random_buf(&newSeed, sizeof(newSeed));
    }
    m_seed = newSeed;

    // Re-fetch base AFTER ensureSpace
    uint8_t* base = m_dataRegion.mutableBase();

    // Write new ItemSizeHolder at base+4
    std::memcpy(base + 4, &newSeed, 4);

    // Write prepared data at base+8
    if (!preparedData.empty()) {
        std::memcpy(base + 8, preparedData.data(), preparedData.size());
    }

    // Compute fresh CRC over [base+4, base+4+newActualSize)
    uint32_t newCRC = computeCRC(base, newActualSize);

    // Update meta
    m_meta.m_actualSize               = newActualSize;
    m_meta.m_crcDigest                = newCRC;
    m_meta.m_sequence++;
    m_meta.lastConfirmedActualSize    = newActualSize;
    m_meta.lastConfirmedCRCDigest     = newCRC;

    // Sync oldStyleActualSize at base[0,4)
    std::memcpy(base, &newActualSize, 4);

    writeMetaInfo(m_crcFd, m_meta);

    // Rebuild m_map from the freshly written region
    m_map = decodeMap(base, m_meta.m_actualSize, false);

    return true;
    // lockGuard destructor releases ExclusiveLock here
}

// ============================================================
// setX implementations — T36
// ============================================================

bool QuickStoreWriter::setBool(const std::string& key, bool value) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeBool(value, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setInt32(const std::string& key, int32_t value) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeInt32(value, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setInt64(const std::string& key, int64_t value) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeInt64(value, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setUInt64(const std::string& key, uint64_t value) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeUInt64(value, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setFloat(const std::string& key, float value) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeFloat(value, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setDouble(const std::string& key, double value) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeDouble(value, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setString(const std::string& key, std::string_view value) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeString(value, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setBytes(const std::string& key, const uint8_t* data, size_t len) {
    if (m_closed) return false;
    std::vector<uint8_t> blob;
    encodeBytes(data, len, blob);
    return appendRecord(key, blob);
}

bool QuickStoreWriter::setBytes(const std::string& key, const std::vector<uint8_t>& data) {
    return setBytes(key, data.data(), data.size());
}

bool QuickStoreWriter::remove(const std::string& key) {
    if (m_closed) return false;
    if (!m_map.count(key)) return false;
    return appendRecord(key, {});  // empty blob = tombstone
}

bool QuickStoreWriter::clearAll() {
    if (m_closed) return false;
    m_map.clear();
    return doFullWriteback();
}

bool QuickStoreWriter::compact() {
    if (m_closed) return false;
    return doFullWriteback();
}

// ============================================================
// Read-through getX — T36
// ============================================================

bool QuickStoreWriter::contains(const std::string& key) const {
    return m_map.count(key) > 0;
}

size_t QuickStoreWriter::count() const {
    return m_map.size();
}

std::vector<std::string> QuickStoreWriter::allKeys() const {
    std::vector<std::string> keys;
    keys.reserve(m_map.size());
    for (const auto& kv : m_map) {
        keys.push_back(kv.first);
    }
    return keys;
}

std::optional<bool> QuickStoreWriter::getBool(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), it->second);
    if (!ptr) return std::nullopt;
    return decodeBool(ptr, len);
}

std::optional<int32_t> QuickStoreWriter::getInt32(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), it->second);
    if (!ptr) return std::nullopt;
    return decodeInt32(ptr, len);
}

std::optional<int64_t> QuickStoreWriter::getInt64(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), it->second);
    if (!ptr) return std::nullopt;
    return decodeInt64(ptr, len);
}

std::optional<uint64_t> QuickStoreWriter::getUInt64(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), it->second);
    if (!ptr) return std::nullopt;
    return decodeUInt64(ptr, len);
}

std::optional<double> QuickStoreWriter::getDouble(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), it->second);
    if (!ptr) return std::nullopt;
    return decodeDouble(ptr, len);
}

std::optional<std::string> QuickStoreWriter::getString(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), it->second);
    if (!ptr) return std::nullopt;
    return decodeString(ptr, len);
}

std::optional<MMBuffer> QuickStoreWriter::getBytes(const std::string& key) const {
    auto it = m_map.find(key);
    if (it == m_map.end()) return std::nullopt;
    auto [ptr, len] = detail::getValueBlob(m_dataRegion.base(), m_dataRegion.capacity(), it->second);
    if (!ptr) return std::nullopt;
    return decodeBytes(ptr, len);
}

} // namespace quickstore
