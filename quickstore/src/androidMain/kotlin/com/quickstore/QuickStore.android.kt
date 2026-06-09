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
}
