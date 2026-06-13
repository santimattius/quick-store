package com.quickstore

/**
 * A fast, MMKV-compatible key-value store for Kotlin Multiplatform.
 *
 * `QuickStore` is a thin [expect] wrapper over a frozen C++ MMKV-compatible
 * storage engine, exposing one type-safe API on Android (AAR + JNI) and iOS
 * (XCFramework). All operations are synchronous; no coroutines are required.
 * The C++ core serializes all operations internally with a recursive mutex;
 * concurrent access to the same instance from multiple threads is safe.
 *
 * Contract shared by every accessor:
 * - Getters return `null` when the key is absent (or stored under a different type).
 * - Setters return `true` on a successful write, `false` otherwise.
 * - Keys are arbitrary non-empty strings; values are typed primitives or raw bytes.
 *
 * The instance owns native resources and implements [AutoCloseable]; call [close]
 * (or use `use { }`) to release them.
 *
 * @param mmkvId the store identifier (logical store name).
 * @param rootDir absolute filesystem path of the directory backing this store.
 * @since 0.1.0
 * @see putString
 * @see QuickStoreSharedPreferences
 */
expect class QuickStore(mmkvId: String, rootDir: String) : AutoCloseable {

    /**
     * Stores [value] under [key], overwriting any existing entry for that key.
     * @param key the non-empty string key to write under.
     * @param value the boolean value to store.
     * @return `true` if the write succeeded, `false` otherwise.
     * @see getBool
     */
    fun setBool(key: String, value: Boolean): Boolean

    /**
     * Stores [value] under [key], overwriting any existing entry for that key.
     * @param key the non-empty string key to write under.
     * @param value the 64-bit integer value to store.
     * @return `true` if the write succeeded, `false` otherwise.
     * @see getLong
     */
    fun setLong(key: String, value: Long): Boolean

    /**
     * Stores [value] under [key], overwriting any existing entry for that key.
     * @param key the non-empty string key to write under.
     * @param value the 64-bit float value to store.
     * @return `true` if the write succeeded, `false` otherwise.
     * @see getDouble
     */
    fun setDouble(key: String, value: Double): Boolean

    /**
     * Stores the raw byte array [value] under [key], overwriting any existing entry.
     * @param key the non-empty string key to write under.
     * @param value the raw byte array to store; an empty array is a valid value.
     * @return `true` if the write succeeded, `false` otherwise.
     * @see getBytes
     * @see putString
     */
    fun setBytes(key: String, value: ByteArray): Boolean

    /**
     * Retrieves the boolean stored under [key].
     * @param key the key to look up.
     * @return the stored value, or `null` if the key is absent or holds another type.
     * @see setBool
     */
    fun getBool(key: String): Boolean?

    /**
     * Retrieves the long stored under [key].
     * @param key the key to look up.
     * @return the stored value, or `null` if the key is absent or holds another type.
     * @see setLong
     */
    fun getLong(key: String): Long?

    /**
     * Retrieves the double stored under [key].
     * @param key the key to look up.
     * @return the stored value, or `null` if the key is absent or holds another type.
     * @see setDouble
     */
    fun getDouble(key: String): Double?

    /**
     * Retrieves the raw byte array stored under [key].
     * @param key the key to look up.
     * @return the stored value, or `null` if the key is absent or holds another type.
     * @see setBytes
     */
    fun getBytes(key: String): ByteArray?

    /**
     * Returns `true` if the store contains an entry for [key], `false` otherwise.
     * @param key the key to check.
     */
    fun contains(key: String): Boolean

    /**
     * Removes the entry associated with [key].
     * @param key the key whose entry should be removed.
     * @return `true` if the entry was present and removed, `false` if the key did not exist.
     */
    fun remove(key: String): Boolean

    /**
     * Returns the total number of entries currently stored.
     */
    fun count(): Long

    /**
     * Returns all keys currently stored in this instance.
     */
    fun allKeys(): List<String>

    /**
     * Compacts the on-disk write-log, reclaiming unused storage space.
     *
     * This is a no-op if the log is already compact. Equivalent to MMKV `trim()`.
     * Call periodically on write-heavy workloads to keep file size bounded.
     */
    fun trim()

    /**
     * Removes all entries from the store.
     */
    fun clear()

    /**
     * Releases native resources held by this store.
     *
     * After `close()`, the store must not be used. Prefer `use { }` for
     * scoped lifetime management. This method is idempotent on the JVM
     * platform; behavior on double-close on iOS follows the platform actual.
     */
    override fun close()

    /**
     * Retrieves all longs for [keys] in a single native round-trip.
     * @param keys the list of keys to retrieve; duplicate keys are deduplicated by the map result.
     * @return a map from each requested key to its value; absent keys map to `null`.
     * @see batchSetLongs
     *
     * Experimental: opt-in required — see [ExperimentalQuickStoreApi] for the
     * stability policy.
     */
    @ExperimentalQuickStoreApi
    fun batchGetLongs(keys: List<String>): Map<String, Long?>

    /**
     * Retrieves all booleans for [keys] in a single native round-trip.
     * @param keys the list of keys to retrieve; absent keys map to `null` in the result.
     * @return a map from each requested key to its value; absent keys map to `null`.
     * @see batchSetBools
     *
     * Experimental: opt-in required — see [ExperimentalQuickStoreApi] for the
     * stability policy.
     */
    @ExperimentalQuickStoreApi
    fun batchGetBools(keys: List<String>): Map<String, Boolean?>

    /**
     * Retrieves all doubles for [keys] in a single native round-trip.
     * @param keys the list of keys to retrieve; absent keys map to `null` in the result.
     * @return a map from each requested key to its value; absent keys map to `null`.
     * @see batchSetDoubles
     *
     * Experimental: opt-in required — see [ExperimentalQuickStoreApi] for the
     * stability policy.
     */
    @ExperimentalQuickStoreApi
    fun batchGetDoubles(keys: List<String>): Map<String, Double?>

    /**
     * Writes all [pairs] in a single native round-trip.
     * @param pairs a map of key-to-value entries; each entry overwrites any existing value for that key.
     * @see batchGetLongs
     *
     * Experimental: opt-in required — see [ExperimentalQuickStoreApi] for the
     * stability policy.
     */
    @ExperimentalQuickStoreApi
    fun batchSetLongs(pairs: Map<String, Long>)

    /**
     * Writes all [pairs] in a single native round-trip.
     * @param pairs a map of key-to-value entries; each entry overwrites any existing value for that key.
     * @see batchGetBools
     *
     * Experimental: opt-in required — see [ExperimentalQuickStoreApi] for the
     * stability policy.
     */
    @ExperimentalQuickStoreApi
    fun batchSetBools(pairs: Map<String, Boolean>)

    /**
     * Writes all [pairs] in a single native round-trip.
     * @param pairs a map of key-to-value entries; each entry overwrites any existing value for that key.
     * @see batchGetDoubles
     *
     * Experimental: opt-in required — see [ExperimentalQuickStoreApi] for the
     * stability policy.
     */
    @ExperimentalQuickStoreApi
    fun batchSetDoubles(pairs: Map<String, Double>)
}
