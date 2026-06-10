package com.quickstore

actual class QuickStore actual constructor(
    val mmkvId: String,
    val rootDir: String,
) : AutoCloseable {

    private val bools   = LinkedHashMap<String, Boolean>()
    private val longs   = LinkedHashMap<String, Long>()
    private val doubles = LinkedHashMap<String, Double>()
    private val bytes   = LinkedHashMap<String, ByteArray>()

    private fun purge(key: String) {
        bools.remove(key); longs.remove(key); doubles.remove(key); bytes.remove(key)
    }

    actual fun setBool(key: String, value: Boolean): Boolean   { purge(key); bools[key]   = value; return true }
    actual fun setLong(key: String, value: Long): Boolean      { purge(key); longs[key]   = value; return true }
    actual fun setDouble(key: String, value: Double): Boolean  { purge(key); doubles[key] = value; return true }
    actual fun setBytes(key: String, value: ByteArray): Boolean { purge(key); bytes[key]   = value; return true }

    actual fun getBool(key: String): Boolean?    = bools[key]
    actual fun getLong(key: String): Long?       = longs[key]
    actual fun getDouble(key: String): Double?   = doubles[key]
    actual fun getBytes(key: String): ByteArray? = bytes[key]

    actual fun contains(key: String): Boolean =
        bools.containsKey(key) || longs.containsKey(key) ||
        doubles.containsKey(key) || bytes.containsKey(key)

    actual fun remove(key: String): Boolean { val had = contains(key); purge(key); return had }

    actual fun count(): Long =
        (bools.keys + longs.keys + doubles.keys + bytes.keys).distinct().size.toLong()

    actual fun allKeys(): List<String> =
        (bools.keys + longs.keys + doubles.keys + bytes.keys).distinct()

    actual fun trim() { /* no-op in fake */ }

    actual fun clear() { bools.clear(); longs.clear(); doubles.clear(); bytes.clear() }

    actual override fun close() { /* no-op in fake */ }
}
