package com.quickstore

expect fun createTestStore(): QuickStore

class FakeQuickStore(val mmkvId: String, val rootDir: String) : AutoCloseable {

    private val bools   = LinkedHashMap<String, Boolean>()
    private val longs   = LinkedHashMap<String, Long>()
    private val doubles = LinkedHashMap<String, Double>()
    private val bytes   = LinkedHashMap<String, ByteArray>()

    private fun purge(key: String) {
        bools.remove(key); longs.remove(key); doubles.remove(key); bytes.remove(key)
    }

    fun setBool(key: String, value: Boolean): Boolean   { purge(key); bools[key]   = value; return true }
    fun setLong(key: String, value: Long): Boolean      { purge(key); longs[key]   = value; return true }
    fun setDouble(key: String, value: Double): Boolean  { purge(key); doubles[key] = value; return true }
    fun setBytes(key: String, value: ByteArray): Boolean { purge(key); bytes[key]   = value; return true }

    fun getBool(key: String): Boolean?    = bools[key]
    fun getLong(key: String): Long?       = longs[key]
    fun getDouble(key: String): Double?   = doubles[key]
    fun getBytes(key: String): ByteArray? = bytes[key]

    fun contains(key: String): Boolean =
        bools.containsKey(key) || longs.containsKey(key) ||
        doubles.containsKey(key) || bytes.containsKey(key)

    fun remove(key: String): Boolean { val had = contains(key); purge(key); return had }

    fun count(): Long =
        (bools.keys + longs.keys + doubles.keys + bytes.keys).distinct().size.toLong()

    fun allKeys(): List<String> =
        (bools.keys + longs.keys + doubles.keys + bytes.keys).distinct()

    fun trim() {}
    fun clear() { bools.clear(); longs.clear(); doubles.clear(); bytes.clear() }
    override fun close() {}
}
