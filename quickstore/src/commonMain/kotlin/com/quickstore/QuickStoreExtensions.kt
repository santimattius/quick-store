package com.quickstore

// Extension functions over QuickStore primitives — no expect/actual needed.
//
// All convenience methods are pure composition over the existing primitives
// (setBytes/getBytes/setLong/getLong/setDouble/getDouble/getBool), which are
// already declared on the expect class and are identical on both platforms.
// There is zero platform-specific logic here; extensions compile once in
// commonMain and call through the existing actual implementations.
//
// String encoding: encodeToByteArray() / decodeToString() are Kotlin stdlib
// commonMain and are UTF-8 by spec on every platform. Charsets.UTF_8 is
// JVM-only and must NOT be used here.

// --- String (UTF-8 via stdlib, no platform encoding) ---

/**
 * Stores [value] as a UTF-8 byte sequence under [key].
 *
 * Delegates to [QuickStore.setBytes] after encoding via [String.encodeToByteArray].
 * @param key the non-empty string key to write under.
 * @param value the string to encode as UTF-8 bytes and store.
 * @return `true` if the write succeeded, `false` otherwise.
 * @see getString
 */
fun QuickStore.putString(key: String, value: String): Boolean =
    setBytes(key, value.encodeToByteArray())

/**
 * Retrieves the UTF-8 string stored under [key].
 *
 * Decodes the raw bytes via [ByteArray.decodeToString].
 * @param key the key to look up.
 * @return the stored string, or `null` if the key is absent or holds a non-bytes type.
 * @see putString
 */
fun QuickStore.getString(key: String): String? =
    getBytes(key)?.decodeToString()

/**
 * Retrieves the UTF-8 string stored under [key], falling back to [default] if absent.
 *
 * Unlike the single-argument overload which returns `null` on a miss, this overload
 * always returns a non-null value — either the stored string or [default].
 * @param key the key to look up.
 * @param default the value to return when the key is absent or holds a non-bytes type.
 * @return the stored string, or [default] if the key is absent or holds a non-bytes type.
 * @see getString
 */
fun QuickStore.getString(key: String, default: String): String =
    getString(key) ?: default

// --- Int (derived from Long; C ABI has no i32) ---

/**
 * Stores [value] as a 64-bit integer under [key] (promotes Int to Long).
 *
 * Delegates to [QuickStore.setLong].
 * @param key the non-empty string key to write under.
 * @param value the integer value to store (promoted to Long in the native layer).
 * @return `true` if the write succeeded, `false` otherwise.
 * @see getInt
 */
fun QuickStore.putInt(key: String, value: Int): Boolean =
    setLong(key, value.toLong())

/**
 * Retrieves the integer stored under [key] (narrows Long to Int).
 *
 * @param key the key to look up.
 * @return the stored value truncated to Int, or `null` if the key is absent or holds a non-long type.
 * @see putInt
 */
fun QuickStore.getInt(key: String): Int? =
    getLong(key)?.toInt()

/**
 * Retrieves the integer stored under [key], falling back to [default] if absent.
 *
 * Unlike the single-argument overload which returns `null` on a miss, this overload
 * always returns a non-null value — either the stored integer or [default].
 * @param key the key to look up.
 * @param default the value to return when the key is absent or holds a non-long type.
 * @return the stored value truncated to Int, or [default] if the key is absent or holds a non-long type.
 * @see getInt
 */
fun QuickStore.getInt(key: String, default: Int): Int =
    getInt(key) ?: default

// --- Float (derived from Double; C ABI has no float) ---

/**
 * Stores [value] as a 64-bit double under [key] (promotes Float to Double).
 *
 * Delegates to [QuickStore.setDouble].
 * @param key the non-empty string key to write under.
 * @param value the float value to store (promoted to Double in the native layer).
 * @return `true` if the write succeeded, `false` otherwise.
 * @see getFloat
 */
fun QuickStore.putFloat(key: String, value: Float): Boolean =
    setDouble(key, value.toDouble())

/**
 * Retrieves the float stored under [key] (narrows Double to Float).
 *
 * @param key the key to look up.
 * @return the stored value narrowed to Float, or `null` if the key is absent or holds a non-double type.
 * @see putFloat
 */
fun QuickStore.getFloat(key: String): Float? =
    getDouble(key)?.toFloat()

/**
 * Retrieves the float stored under [key], falling back to [default] if absent.
 *
 * Unlike the single-argument overload which returns `null` on a miss, this overload
 * always returns a non-null value — either the stored float or [default].
 * @param key the key to look up.
 * @param default the value to return when the key is absent or holds a non-double type.
 * @return the stored value narrowed to Float, or [default] if the key is absent or holds a non-double type.
 * @see getFloat
 */
fun QuickStore.getFloat(key: String, default: Float): Float =
    getFloat(key) ?: default

// --- Default-value overloads for existing nullable getters ---
// These are OVERLOADS (different arity) — no clash with the nullable single-arg
// members declared in the expect class (getBool(key): Boolean?, etc.).

/**
 * Retrieves the boolean stored under [key], falling back to [default] if absent.
 *
 * Unlike [QuickStore.getBool] which returns `null` on a miss, this overload
 * always returns a non-null value — either the stored boolean or [default].
 * @param key the key to look up.
 * @param default the value to return when the key is absent or holds a non-boolean type.
 * @return the stored value, or [default] if the key is absent or holds a non-boolean type.
 * @see QuickStore.getBool
 */
fun QuickStore.getBool(key: String, default: Boolean): Boolean =
    getBool(key) ?: default

/**
 * Retrieves the long stored under [key], falling back to [default] if absent.
 *
 * Unlike [QuickStore.getLong] which returns `null` on a miss, this overload
 * always returns a non-null value — either the stored long or [default].
 * @param key the key to look up.
 * @param default the value to return when the key is absent or holds a non-long type.
 * @return the stored value, or [default] if the key is absent or holds a non-long type.
 * @see QuickStore.getLong
 */
fun QuickStore.getLong(key: String, default: Long): Long =
    getLong(key) ?: default

/**
 * Retrieves the double stored under [key], falling back to [default] if absent.
 *
 * Unlike [QuickStore.getDouble] which returns `null` on a miss, this overload
 * always returns a non-null value — either the stored double or [default].
 * @param key the key to look up.
 * @param default the value to return when the key is absent or holds a non-double type.
 * @return the stored value, or [default] if the key is absent or holds a non-double type.
 * @see QuickStore.getDouble
 */
fun QuickStore.getDouble(key: String, default: Double): Double =
    getDouble(key) ?: default
