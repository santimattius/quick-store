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

fun QuickStore.putString(key: String, value: String): Boolean =
    setBytes(key, value.encodeToByteArray())

fun QuickStore.getString(key: String): String? =
    getBytes(key)?.decodeToString()

fun QuickStore.getString(key: String, default: String): String =
    getString(key) ?: default

// --- Int (derived from Long; C ABI has no i32) ---

fun QuickStore.putInt(key: String, value: Int): Boolean =
    setLong(key, value.toLong())

fun QuickStore.getInt(key: String): Int? =
    getLong(key)?.toInt()

fun QuickStore.getInt(key: String, default: Int): Int =
    getInt(key) ?: default

// --- Float (derived from Double; C ABI has no float) ---

fun QuickStore.putFloat(key: String, value: Float): Boolean =
    setDouble(key, value.toDouble())

fun QuickStore.getFloat(key: String): Float? =
    getDouble(key)?.toFloat()

fun QuickStore.getFloat(key: String, default: Float): Float =
    getFloat(key) ?: default

// --- Default-value overloads for existing nullable getters ---
// These are OVERLOADS (different arity) — no clash with the nullable single-arg
// members declared in the expect class (getBool(key): Boolean?, etc.).

fun QuickStore.getBool(key: String, default: Boolean): Boolean =
    getBool(key) ?: default

fun QuickStore.getLong(key: String, default: Long): Long =
    getLong(key) ?: default

fun QuickStore.getDouble(key: String, default: Double): Double =
    getDouble(key) ?: default
