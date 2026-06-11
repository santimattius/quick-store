package com.quickstore

import java.io.Closeable
import java.io.IOException

actual class QuickStore actual constructor(
    mmkvId: String,
    rootDir: String
) : Closeable {

    private var _handle: Long

    init {
        System.loadLibrary("quickstore_jni")
        _handle = nativeOpen(mmkvId, rootDir)
        if (_handle == 0L) {
            throw IOException("Failed to open QuickStore: id=$mmkvId, dir=$rootDir")
        }
    }

    actual fun setBool(key: String, value: Boolean): Boolean =
        nativeSetBool(_handle, key, value)

    actual fun setLong(key: String, value: Long): Boolean =
        nativeSetLong(_handle, key, value)

    actual fun setDouble(key: String, value: Double): Boolean =
        nativeSetDouble(_handle, key, value)

    actual fun setBytes(key: String, value: ByteArray): Boolean =
        nativeSetBytes(_handle, key, value)

    actual fun getBool(key: String): Boolean? {
        val out = BooleanArray(1)
        return if (nativeGetBool(_handle, key, out)) out[0] else null
    }

    actual fun getLong(key: String): Long? {
        val out = LongArray(1)
        return if (nativeGetLong(_handle, key, out)) out[0] else null
    }

    actual fun getDouble(key: String): Double? {
        val out = DoubleArray(1)
        return if (nativeGetDouble(_handle, key, out)) out[0] else null
    }

    actual fun getBytes(key: String): ByteArray? =
        nativeGetBytes(_handle, key)

    actual fun contains(key: String): Boolean =
        nativeContains(_handle, key)

    actual fun remove(key: String): Boolean =
        nativeRemove(_handle, key)

    actual fun count(): Long =
        nativeCount(_handle)

    actual fun allKeys(): List<String> =
        nativeAllKeys(_handle).toList()

    actual fun trim() =
        nativeTrim(_handle)

    actual fun clear() =
        nativeClear(_handle)

    actual override fun close() {
        if (_handle != 0L) {
            nativeClose(_handle)
            _handle = 0L
        }
    }

    @ExperimentalQuickStoreApi
    actual fun batchGetLongs(keys: List<String>): Map<String, Long?> {
        if (keys.isEmpty()) return emptyMap()
        val result = LinkedHashMap<String, Long?>(keys.size)
        keys.chunked(500).forEach { chunk ->
            val arr = chunk.toTypedArray()
            val outValues = LongArray(arr.size)
            val outFound = BooleanArray(arr.size)
            nativeGetLongs(_handle, arr, outValues, outFound)
            arr.forEachIndexed { i, key ->
                result[key] = if (outFound[i]) outValues[i] else null
            }
        }
        return result
    }

    @ExperimentalQuickStoreApi
    actual fun batchGetBools(keys: List<String>): Map<String, Boolean?> {
        if (keys.isEmpty()) return emptyMap()
        val result = LinkedHashMap<String, Boolean?>(keys.size)
        keys.chunked(500).forEach { chunk ->
            val arr = chunk.toTypedArray()
            val outValues = BooleanArray(arr.size)
            val outFound = BooleanArray(arr.size)
            nativeGetBools(_handle, arr, outValues, outFound)
            arr.forEachIndexed { i, key ->
                result[key] = if (outFound[i]) outValues[i] else null
            }
        }
        return result
    }

    @ExperimentalQuickStoreApi
    actual fun batchGetDoubles(keys: List<String>): Map<String, Double?> {
        if (keys.isEmpty()) return emptyMap()
        val result = LinkedHashMap<String, Double?>(keys.size)
        keys.chunked(500).forEach { chunk ->
            val arr = chunk.toTypedArray()
            val outValues = DoubleArray(arr.size)
            val outFound = BooleanArray(arr.size)
            nativeGetDoubles(_handle, arr, outValues, outFound)
            arr.forEachIndexed { i, key ->
                result[key] = if (outFound[i]) outValues[i] else null
            }
        }
        return result
    }

    @ExperimentalQuickStoreApi
    actual fun batchSetLongs(pairs: Map<String, Long>) {
        if (pairs.isEmpty()) return
        pairs.entries.chunked(500).forEach { chunk ->
            nativeSetLongs(
                _handle,
                chunk.map { it.key }.toTypedArray(),
                chunk.map { it.value }.toLongArray()
            )
        }
    }

    @ExperimentalQuickStoreApi
    actual fun batchSetBools(pairs: Map<String, Boolean>) {
        if (pairs.isEmpty()) return
        pairs.entries.chunked(500).forEach { chunk ->
            nativeSetBools(
                _handle,
                chunk.map { it.key }.toTypedArray(),
                chunk.map { it.value }.toBooleanArray()
            )
        }
    }

    @ExperimentalQuickStoreApi
    actual fun batchSetDoubles(pairs: Map<String, Double>) {
        if (pairs.isEmpty()) return
        pairs.entries.chunked(500).forEach { chunk ->
            nativeSetDoubles(
                _handle,
                chunk.map { it.key }.toTypedArray(),
                chunk.map { it.value }.toDoubleArray()
            )
        }
    }

    // JNI externals
    private external fun nativeOpen(mmkvId: String, rootDir: String): Long
    private external fun nativeClose(handle: Long)
    private external fun nativeSetBool(handle: Long, key: String, value: Boolean): Boolean
    private external fun nativeSetLong(handle: Long, key: String, value: Long): Boolean
    private external fun nativeSetDouble(handle: Long, key: String, value: Double): Boolean
    private external fun nativeSetBytes(handle: Long, key: String, value: ByteArray): Boolean
    private external fun nativeGetBool(handle: Long, key: String, out: BooleanArray): Boolean
    private external fun nativeGetLong(handle: Long, key: String, out: LongArray): Boolean
    private external fun nativeGetDouble(handle: Long, key: String, out: DoubleArray): Boolean
    private external fun nativeGetBytes(handle: Long, key: String): ByteArray?
    private external fun nativeContains(handle: Long, key: String): Boolean
    private external fun nativeRemove(handle: Long, key: String): Boolean
    private external fun nativeCount(handle: Long): Long
    private external fun nativeAllKeys(handle: Long): Array<String>
    private external fun nativeTrim(handle: Long)
    private external fun nativeClear(handle: Long)
    private external fun nativeGetLongs(
        handle: Long,
        keys: Array<String>,
        outValues: LongArray,
        outFound: BooleanArray
    )
    private external fun nativeGetBools(
        handle: Long,
        keys: Array<String>,
        outValues: BooleanArray,
        outFound: BooleanArray
    )
    private external fun nativeGetDoubles(
        handle: Long,
        keys: Array<String>,
        outValues: DoubleArray,
        outFound: BooleanArray
    )
    private external fun nativeSetLongs(handle: Long, keys: Array<String>, values: LongArray)
    private external fun nativeSetBools(handle: Long, keys: Array<String>, values: BooleanArray)
    private external fun nativeSetDoubles(handle: Long, keys: Array<String>, values: DoubleArray)
}
