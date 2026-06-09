#include "quickstore/kv_store.h"
#include "quickstore/writer.h"
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <memory>
#include <sys/stat.h>

// ============================================================
// Opaque handle — body defined ONLY here, never in the header.
// ============================================================

struct kv_store {
    std::unique_ptr<quickstore::QuickStoreWriter> writer;
    mutable std::recursive_mutex mu;
    bool closed = false;
};

// ============================================================
// Internal helpers
// ============================================================

static kv_status check_store(const kv_store* s) {
    if (!s) return KV_INVALID_ARG;
    return KV_OK;
}

// ============================================================
// Lifecycle
// ============================================================

kv_status kv_open(const char* mmkv_id, const char* root_dir,
                  const kv_options* opts, kv_store** out_store) {
    if (!mmkv_id || !root_dir || !out_store) return KV_INVALID_ARG;

    *out_store = nullptr;

    bool mp = (opts != nullptr && opts->multi_process != 0);
    auto w = quickstore::QuickStoreWriter::open(
        std::string(mmkv_id),
        std::string(root_dir),
        0,
        mp);

    if (!w) {
        // Distinguish CRC corruption (file exists but data invalid) from I/O failure.
        struct stat st{};
        std::string dataPath = std::string(root_dir) + "/" + std::string(mmkv_id);
        return (::stat(dataPath.c_str(), &st) == 0) ? KV_CORRUPT : KV_IO;
    }

    kv_store* s = new kv_store();
    s->writer   = std::make_unique<quickstore::QuickStoreWriter>(std::move(*w));
    s->closed   = false;

    *out_store = s;
    return KV_OK;
}

kv_status kv_close(kv_store* s) {
    if (!s) return KV_INVALID_ARG;

    {
        std::lock_guard<std::recursive_mutex> lk(s->mu);
        if (!s->closed) {
            s->closed = true;
            s->writer->close();
        }
    }
    // delete AFTER releasing the lock — deleting with lock held is UB
    delete s;
    return KV_OK;
}

// ============================================================
// Mutating operations
// ============================================================

kv_status kv_set_bool(kv_store* s, const char* key, int value) {
    if (!s || !key) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    return s->writer->setBool(std::string(key), static_cast<bool>(value))
        ? KV_OK : KV_IO;
}

kv_status kv_set_i64(kv_store* s, const char* key, int64_t value) {
    if (!s || !key) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    return s->writer->setInt64(std::string(key), value) ? KV_OK : KV_IO;
}

kv_status kv_set_double(kv_store* s, const char* key, double value) {
    if (!s || !key) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    return s->writer->setDouble(std::string(key), value) ? KV_OK : KV_IO;
}

kv_status kv_set_bytes(kv_store* s, const char* key,
                       const uint8_t* data, size_t len) {
    if (!s || !key) return KV_INVALID_ARG;
    if (!data && len > 0) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    // data == NULL && len == 0 is valid (empty bytes)
    const uint8_t* p = data ? data : reinterpret_cast<const uint8_t*>("");
    return s->writer->setBytes(std::string(key), p, len) ? KV_OK : KV_IO;
}

// ============================================================
// Query operations
// ============================================================

kv_status kv_get_bool(kv_store* s, const char* key, int* out_value) {
    if (!s || !key || !out_value) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    auto v = s->writer->getBool(std::string(key));
    if (!v) return KV_NOT_FOUND;
    *out_value = *v ? 1 : 0;
    return KV_OK;
}

kv_status kv_get_i64(kv_store* s, const char* key, int64_t* out_value) {
    if (!s || !key || !out_value) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    auto v = s->writer->getInt64(std::string(key));
    if (!v) return KV_NOT_FOUND;
    *out_value = *v;
    return KV_OK;
}

kv_status kv_get_double(kv_store* s, const char* key, double* out_value) {
    if (!s || !key || !out_value) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    auto v = s->writer->getDouble(std::string(key));
    if (!v) return KV_NOT_FOUND;
    *out_value = *v;
    return KV_OK;
}

kv_status kv_get_bytes(kv_store* s, const char* key, kv_buffer* out_buf) {
    if (!s || !key || !out_buf) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    auto v = s->writer->getBytes(std::string(key));
    if (!v) return KV_NOT_FOUND;

    size_t sz = v->size();
    if (sz == 0) {
        out_buf->data = nullptr;
        out_buf->len  = 0;
        return KV_OK;
    }

    uint8_t* buf = static_cast<uint8_t*>(std::malloc(sz));
    if (!buf) return KV_IO;
    std::memcpy(buf, v->data(), sz);
    out_buf->data = buf;
    out_buf->len  = sz;
    return KV_OK;
}

kv_status kv_get_into(kv_store* s, const char* key,
                      uint8_t* buf, size_t buf_len, size_t* out_written) {
    if (!s || !key || !buf || !out_written) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    auto v = s->writer->getBytes(std::string(key));
    if (!v) return KV_NOT_FOUND;
    size_t sz = v->size();
    if (sz > buf_len) return KV_BUFFER_TOO_SMALL;  // no partial copy
    if (sz > 0) std::memcpy(buf, v->data(), sz);
    *out_written = sz;
    return KV_OK;
}

// ============================================================
// Index and bulk operations
// ============================================================

kv_status kv_contains(kv_store* s, const char* key, int* out_found) {
    if (!s || !key || !out_found) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    *out_found = s->writer->contains(std::string(key)) ? 1 : 0;
    return KV_OK;
}

kv_status kv_remove(kv_store* s, const char* key) {
    if (!s || !key) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    return s->writer->remove(std::string(key)) ? KV_OK : KV_IO;
}

kv_status kv_count(kv_store* s, size_t* out_count) {
    if (!s || !out_count) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    *out_count = s->writer->count();
    return KV_OK;
}

kv_status kv_all_keys(kv_store* s, kv_buffer* out_keys, size_t* out_count) {
    if (!s || !out_keys || !out_count) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;

    auto keys = s->writer->allKeys();
    *out_count = keys.size();

    if (keys.empty()) {
        out_keys->data = nullptr;
        out_keys->len  = 0;
        return KV_OK;
    }

    // Compute total: each key + NUL terminator
    size_t total = 0;
    for (const auto& k : keys) total += k.size() + 1;

    uint8_t* buf = static_cast<uint8_t*>(std::malloc(total));
    if (!buf) return KV_IO;

    uint8_t* p = buf;
    for (const auto& k : keys) {
        std::memcpy(p, k.data(), k.size());
        p += k.size();
        *p++ = '\0';
    }

    out_keys->data = buf;
    out_keys->len  = total;
    return KV_OK;
}

kv_status kv_trim(kv_store* s) {
    if (!s) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    return s->writer->compact() ? KV_OK : KV_IO;
}

kv_status kv_clear(kv_store* s) {
    if (!s) return KV_INVALID_ARG;
    std::lock_guard<std::recursive_mutex> lk(s->mu);
    if (s->closed) return KV_CLOSED;
    return s->writer->clearAll() ? KV_OK : KV_IO;
}

// ============================================================
// Memory management
// ============================================================

void kv_buffer_free(kv_buffer* buf) {
    if (!buf) return;
    std::free(buf->data);
    buf->data = nullptr;
    buf->len  = 0;
}
