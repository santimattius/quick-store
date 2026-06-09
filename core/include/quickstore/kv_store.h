#pragma once

#if defined(__GNUC__) || defined(__clang__)
#  define QUICKSTORE_EXPORT __attribute__((visibility("default")))
#elif defined(_WIN32)
#  ifdef QUICKSTORE_BUILDING_DLL
#    define QUICKSTORE_EXPORT __declspec(dllexport)
#  else
#    define QUICKSTORE_EXPORT __declspec(dllimport)
#  endif
#else
#  define QUICKSTORE_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

/* Opaque handle — struct body defined only in kv_store.cpp. */
typedef struct kv_store kv_store;

/* Status codes returned by all kv_* functions. */
typedef enum {
    KV_OK               = 0,
    KV_NOT_FOUND        = 1,
    KV_IO               = 2,
    KV_INVALID_ARG      = 3,
    KV_BUFFER_TOO_SMALL = 4,
    KV_CLOSED           = 5,
    KV_CORRUPT          = 6
} kv_status;

/* Recovery strategy applied when CRC validation fails on open. */
typedef enum {
    KV_RECOVER_DISCARD = 0,  /* Discard corrupt data (default). */
    KV_RECOVER_GREEDY  = 1   /* Attempt greedy decode on CRC failure. */
} kv_recover_strategy;

/*
 * Open options. All fields ignored in Fase 2 except recover_strategy.
 * Pass NULL to kv_open to use defaults (no encrypt, single-process,
 * KV_RECOVER_DISCARD, no expiry).
 * NOTE: thread safety is single-process only; multi_process is accepted
 * but ignored until Fase 3 adds fcntl inter-process locking.
 */
typedef struct {
    const char*         crypt_key;        /* ignored Fase 2 */
    int                 multi_process;    /* ignored Fase 2 */
    kv_recover_strategy recover_strategy;
    uint32_t            expire_seconds;   /* ignored Fase 2 */
} kv_options;

/*
 * Heap-allocated byte buffer returned by kv_get_bytes and kv_all_keys.
 * Caller MUST free via kv_buffer_free. The kv_buffer struct itself
 * (typically stack-allocated) is NOT freed by kv_buffer_free.
 */
typedef struct {
    uint8_t* data;
    size_t   len;
} kv_buffer;

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/*
 * Opens (or creates) a store identified by mmkv_id under root_dir.
 * mmkv_id == NULL || root_dir == NULL || out_store == NULL -> KV_INVALID_ARG.
 * opts == NULL -> safe defaults.
 * On success: *out_store is heap-allocated; MUST be freed via kv_close.
 * On failure: *out_store = NULL; returns KV_IO.
 */
QUICKSTORE_EXPORT kv_status kv_open(const char* mmkv_id, const char* root_dir,
                                    const kv_options* opts, kv_store** out_store);

/*
 * Closes the store and frees the heap allocation.
 * store == NULL -> KV_INVALID_ARG.
 * The pointer is invalid after this call.
 */
QUICKSTORE_EXPORT kv_status kv_close(kv_store* store);

/* ------------------------------------------------------------------ */
/* Mutating operations                                                 */
/* ------------------------------------------------------------------ */

QUICKSTORE_EXPORT kv_status kv_set_bool  (kv_store* store, const char* key, int value);
QUICKSTORE_EXPORT kv_status kv_set_i64   (kv_store* store, const char* key, int64_t value);
QUICKSTORE_EXPORT kv_status kv_set_double(kv_store* store, const char* key, double value);

/*
 * data == NULL && len == 0  -> KV_OK  (empty bytes value is valid).
 * data == NULL && len  > 0  -> KV_INVALID_ARG.
 */
QUICKSTORE_EXPORT kv_status kv_set_bytes(kv_store* store, const char* key,
                                         const uint8_t* data, size_t len);

/* ------------------------------------------------------------------ */
/* Query operations                                                    */
/* ------------------------------------------------------------------ */

/* out_value == NULL -> KV_INVALID_ARG. Key absent -> KV_NOT_FOUND. */
QUICKSTORE_EXPORT kv_status kv_get_bool  (kv_store* store, const char* key, int* out_value);
QUICKSTORE_EXPORT kv_status kv_get_i64   (kv_store* store, const char* key, int64_t* out_value);
QUICKSTORE_EXPORT kv_status kv_get_double(kv_store* store, const char* key, double* out_value);

/*
 * On success: out_buf->data is heap-allocated; caller MUST call kv_buffer_free.
 * Key absent -> KV_NOT_FOUND.
 */
QUICKSTORE_EXPORT kv_status kv_get_bytes(kv_store* store, const char* key, kv_buffer* out_buf);

/*
 * Zero-alloc variant: copies value bytes into caller-supplied buf.
 * buf_len < stored size -> KV_BUFFER_TOO_SMALL (no partial write).
 * On success: *out_written = bytes copied.
 */
QUICKSTORE_EXPORT kv_status kv_get_into(kv_store* store, const char* key,
                                        uint8_t* buf, size_t buf_len, size_t* out_written);

/* ------------------------------------------------------------------ */
/* Index and bulk operations                                           */
/* ------------------------------------------------------------------ */

/* *out_found = 1 if present, 0 if absent. */
QUICKSTORE_EXPORT kv_status kv_contains(kv_store* store, const char* key, int* out_found);

QUICKSTORE_EXPORT kv_status kv_remove(kv_store* store, const char* key);
QUICKSTORE_EXPORT kv_status kv_count (kv_store* store, size_t* out_count);

/*
 * Empty store: out_keys = {NULL, 0}, *out_count = 0, returns KV_OK.
 * Non-empty: out_keys->data = heap-allocated NUL-separated key strings;
 *            *out_count = number of keys.
 * Caller iterates: ptr = data; for (i=0; i<count; i++) { use ptr; ptr += strlen(ptr)+1; }
 * Free with kv_buffer_free.
 */
QUICKSTORE_EXPORT kv_status kv_all_keys(kv_store* store, kv_buffer* out_keys, size_t* out_count);

/* Compacts the store (full write-back). KV_OK on success, KV_IO on failure. */
QUICKSTORE_EXPORT kv_status kv_trim(kv_store* store);

/* Clears all keys from the store. */
QUICKSTORE_EXPORT kv_status kv_clear(kv_store* store);

/* ------------------------------------------------------------------ */
/* Memory management                                                   */
/* ------------------------------------------------------------------ */

/*
 * Frees buf->data and zeros both fields. Safe to call with buf->data == NULL.
 * The kv_buffer struct itself is NOT freed (caller manages it).
 */
QUICKSTORE_EXPORT void kv_buffer_free(kv_buffer* buf);

#ifdef __cplusplus
}
#endif
