package com.quickstore

import quickstore.cinterop.*
import kotlinx.cinterop.*
import cnames.structs.kv_store as KvStore

@OptIn(ExperimentalForeignApi::class)
actual class QuickStore actual constructor(
    mmkvId: String,
    rootDir: String
) : AutoCloseable {

    private var handle: CPointer<KvStore>?

    init {
        val store = memScoped {
            val out = alloc<CPointerVar<KvStore>>()
            val st = kv_open(mmkvId, rootDir, null, out.ptr)
            if (st != KV_OK) {
                throw IllegalStateException(
                    "Failed to open QuickStore (status=$st): id=$mmkvId, dir=$rootDir"
                )
            }
            out.value
        }
        handle = store
    }

    actual fun setBool(key: String, value: Boolean): Boolean {
        val st = kv_set_bool(handle, key, if (value) 1 else 0)
        return st == KV_OK
    }

    actual fun setLong(key: String, value: Long): Boolean {
        val st = kv_set_i64(handle, key, value)
        return st == KV_OK
    }

    actual fun setDouble(key: String, value: Double): Boolean {
        val st = kv_set_double(handle, key, value)
        return st == KV_OK
    }

    actual fun setBytes(key: String, value: ByteArray): Boolean {
        val st = value.toUByteArray().usePinned { pinned ->
            kv_set_bytes(
                handle, key,
                pinned.addressOf(0),
                value.size.toULong()
            )
        }
        return st == KV_OK
    }

    actual fun getBool(key: String): Boolean? = memScoped {
        val out = alloc<IntVar>()
        val st = kv_get_bool(handle, key, out.ptr)
        if (st == KV_NOT_FOUND) null else out.value != 0
    }

    actual fun getLong(key: String): Long? = memScoped {
        val out = alloc<LongVar>()
        val st = kv_get_i64(handle, key, out.ptr)
        if (st == KV_NOT_FOUND) null else out.value
    }

    actual fun getDouble(key: String): Double? = memScoped {
        val out = alloc<DoubleVar>()
        val st = kv_get_double(handle, key, out.ptr)
        if (st == KV_NOT_FOUND) null else out.value
    }

    actual fun getBytes(key: String): ByteArray? = memScoped {
        val buf = alloc<kv_buffer>()
        buf.data = null
        buf.len = 0u
        val st = kv_get_bytes(handle, key, buf.ptr)
        if (st == KV_NOT_FOUND) {
            null
        } else {
            val data = buf.data
            val len = buf.len.toInt()
            val result = if (data != null && len > 0) {
                data.readBytes(len)
            } else {
                ByteArray(0)
            }
            kv_buffer_free(buf.ptr)
            result
        }
    }

    actual fun contains(key: String): Boolean = memScoped {
        val out = alloc<IntVar>()
        val st = kv_contains(handle, key, out.ptr)
        if (st != KV_OK) false else out.value != 0
    }

    actual fun remove(key: String): Boolean {
        val st = kv_remove(handle, key)
        return st == KV_OK
    }

    actual fun count(): Long = memScoped {
        val out = alloc<ULongVar>()
        val st = kv_count(handle, out.ptr)
        if (st != KV_OK) 0L else out.value.toLong()
    }

    actual fun allKeys(): List<String> = memScoped {
        val buf = alloc<kv_buffer>()
        buf.data = null
        buf.len = 0u
        val countVar = alloc<ULongVar>()
        val st = kv_all_keys(handle, buf.ptr, countVar.ptr)
        if (st != KV_OK) return@memScoped emptyList()

        val count = countVar.value.toInt()
        if (count == 0 || buf.data == null) {
            return@memScoped emptyList()
        }

        val result = ArrayList<String>(count)
        var p: CPointer<ByteVar>? = buf.data!!.reinterpret()
        for (i in 0 until count) {
            val currentP = p ?: break
            val key = currentP.toKString()
            result.add(key)
            // advance past key + NUL terminator
            p = (currentP + key.length + 1)
        }
        kv_buffer_free(buf.ptr)
        result
    }

    @ExperimentalQuickStoreApi
    actual fun batchGetLongs(keys: List<String>): Map<String, Long?> {
        if (keys.isEmpty()) return emptyMap()
        val result = LinkedHashMap<String, Long?>(keys.size)
        keys.chunked(500).forEach { chunk ->
            memScoped {
                val out = alloc<LongVar>()
                chunk.forEach { key ->
                    val st = kv_get_i64(handle, key, out.ptr)
                    result[key] = if (st == KV_NOT_FOUND) null else out.value
                }
            }
        }
        return result
    }

    @ExperimentalQuickStoreApi
    actual fun batchGetBools(keys: List<String>): Map<String, Boolean?> {
        if (keys.isEmpty()) return emptyMap()
        val result = LinkedHashMap<String, Boolean?>(keys.size)
        keys.chunked(500).forEach { chunk ->
            memScoped {
                val out = alloc<IntVar>()
                chunk.forEach { key ->
                    val st = kv_get_bool(handle, key, out.ptr)
                    result[key] = if (st == KV_NOT_FOUND) null else out.value != 0
                }
            }
        }
        return result
    }

    @ExperimentalQuickStoreApi
    actual fun batchGetDoubles(keys: List<String>): Map<String, Double?> {
        if (keys.isEmpty()) return emptyMap()
        val result = LinkedHashMap<String, Double?>(keys.size)
        keys.chunked(500).forEach { chunk ->
            memScoped {
                val out = alloc<DoubleVar>()
                chunk.forEach { key ->
                    val st = kv_get_double(handle, key, out.ptr)
                    result[key] = if (st == KV_NOT_FOUND) null else out.value
                }
            }
        }
        return result
    }

    @ExperimentalQuickStoreApi
    actual fun batchSetLongs(pairs: Map<String, Long>) {
        if (pairs.isEmpty()) return
        pairs.entries.chunked(500).forEach { chunk ->
            memScoped {
                chunk.forEach { (key, value) -> kv_set_i64(handle, key, value) }
            }
        }
    }

    @ExperimentalQuickStoreApi
    actual fun batchSetBools(pairs: Map<String, Boolean>) {
        if (pairs.isEmpty()) return
        pairs.entries.chunked(500).forEach { chunk ->
            memScoped {
                chunk.forEach { (key, value) -> kv_set_bool(handle, key, if (value) 1 else 0) }
            }
        }
    }

    @ExperimentalQuickStoreApi
    actual fun batchSetDoubles(pairs: Map<String, Double>) {
        if (pairs.isEmpty()) return
        pairs.entries.chunked(500).forEach { chunk ->
            memScoped {
                chunk.forEach { (key, value) -> kv_set_double(handle, key, value) }
            }
        }
    }

    actual fun trim() {
        kv_trim(handle)
    }

    actual fun clear() {
        kv_clear(handle)
    }

    actual override fun close() {
        val h = handle
        if (h != null) {
            kv_close(h)
            handle = null
        }
    }
}
