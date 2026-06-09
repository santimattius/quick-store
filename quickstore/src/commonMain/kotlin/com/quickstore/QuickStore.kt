package com.quickstore

expect class QuickStore(mmkvId: String, rootDir: String) : AutoCloseable {
    fun setBool(key: String, value: Boolean): Boolean
    fun setLong(key: String, value: Long): Boolean
    fun setDouble(key: String, value: Double): Boolean
    fun setBytes(key: String, value: ByteArray): Boolean
    fun getBool(key: String): Boolean?
    fun getLong(key: String): Long?
    fun getDouble(key: String): Double?
    fun getBytes(key: String): ByteArray?
    fun contains(key: String): Boolean
    fun remove(key: String): Boolean
    fun count(): Long
    fun allKeys(): List<String>
    fun trim()
    fun clear()
    override fun close()
}
