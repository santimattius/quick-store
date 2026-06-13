package com.quickstore

import android.content.SharedPreferences

/**
 * A drop-in [SharedPreferences] adapter backed by a [QuickStore] instance.
 *
 * Usage:
 *   val prefs: SharedPreferences = QuickStoreSharedPreferences(quickStore)
 *
 * Known gaps (documented, not spec'd):
 *   - getStringSet / putStringSet → throws UnsupportedOperationException (no multi-value encoding in core ABI)
 *   - Change listeners             → throws UnsupportedOperationException (no notification hook in frozen core)
 *   - getAll() type fidelity       → best-effort; not for hot paths (values are untyped at storage layer)
 *   - commit() == apply()          → both synchronous; commit() returns true (no deferred write queue)
 */
class QuickStoreSharedPreferences(private val store: QuickStore) : SharedPreferences {

    // -------------------------------------------------------------------------
    // Read methods
    // -------------------------------------------------------------------------

    override fun getString(key: String, defValue: String?): String? =
        store.getBytes(key)?.decodeToString() ?: defValue

    override fun getInt(key: String, defValue: Int): Int =
        store.getInt(key, defValue)

    override fun getLong(key: String, defValue: Long): Long =
        store.getLong(key, defValue)

    override fun getFloat(key: String, defValue: Float): Float =
        store.getFloat(key, defValue)

    override fun getBoolean(key: String, defValue: Boolean): Boolean =
        store.getBool(key, defValue)

    override fun contains(key: String): Boolean =
        store.contains(key)

    /**
     * Returns a best-effort map of all stored keys and their values.
     *
     * For each key, the type is probed in order: Boolean → Long → Double → ByteArray-as-String.
     * The C core stores values with a type tag, so a Long-typed entry will return null from
     * getBool/getDouble — probing is disambiguated by the core's internal tag, not guesswork.
     * String is last because it is the bytes fallback.
     *
     * NOT intended for hot paths — use for migration or inspection only.
     */
    override fun getAll(): Map<String, Any?> {
        val result = LinkedHashMap<String, Any?>()
        for (key in store.allKeys()) {
            val value: Any? = store.getBool(key)
                ?: store.getLong(key)
                ?: store.getDouble(key)
                ?: store.getBytes(key)?.decodeToString()
            result[key] = value
        }
        return result
    }

    // -------------------------------------------------------------------------
    // Unsupported operations
    // -------------------------------------------------------------------------

    /**
     * Not supported — throws [UnsupportedOperationException].
     *
     * The QuickStore C ABI has no multi-value encoding; string sets cannot be
     * represented without an additional serialization layer.
     * Use [putString] with JSON serialization as an alternative.
     * @return never returns normally.
     * @throws UnsupportedOperationException always.
     */
    override fun getStringSet(key: String, defValues: MutableSet<String>?): MutableSet<String>? =
        throw UnsupportedOperationException("Not supported by QuickStore")

    /**
     * Not supported — throws [UnsupportedOperationException].
     *
     * The frozen C++ core has no notification hook; change listeners cannot be registered.
     * @param listener the listener that would be registered (ignored).
     * @throws UnsupportedOperationException always.
     */
    override fun registerOnSharedPreferenceChangeListener(
        listener: SharedPreferences.OnSharedPreferenceChangeListener
    ): Unit = throw UnsupportedOperationException("Not supported by QuickStore")

    /**
     * Not supported — throws [UnsupportedOperationException].
     *
     * The frozen C++ core has no notification hook; change listeners cannot be unregistered.
     * @param listener the listener that would be unregistered (ignored).
     * @throws UnsupportedOperationException always.
     */
    override fun unregisterOnSharedPreferenceChangeListener(
        listener: SharedPreferences.OnSharedPreferenceChangeListener
    ): Unit = throw UnsupportedOperationException("Not supported by QuickStore")

    // -------------------------------------------------------------------------
    // Editor
    // -------------------------------------------------------------------------

    override fun edit(): SharedPreferences.Editor = Editor()

    /**
     * Adapts [SharedPreferences.Editor] to the QuickStore backend.
     *
     * All mutations are buffered in memory and applied atomically to the underlying
     * [QuickStore] instance when [commit] or [apply] is called. Per the
     * [SharedPreferences] contract, [clear] is executed before any pending puts
     * within the same editor batch.
     *
     * @throws UnsupportedOperationException for [putStringSet] — not supported by the QuickStore C ABI.
     */
    inner class Editor : SharedPreferences.Editor {

        private val pending = LinkedHashMap<String, Any?>()
        private var clearRequested = false

        // Sentinel value to distinguish an explicit remove from an absent key.
        private val REMOVE = object {}

        /**
         * Schedules a string write for [key]. A `null` [value] is treated as a remove
         * per the [SharedPreferences] contract.
         * @return this editor, for chaining.
         */
        override fun putString(key: String, value: String?): SharedPreferences.Editor {
            pending[key] = value
            return this
        }

        /**
         * Schedules an integer write for [key].
         * @return this editor, for chaining.
         */
        override fun putInt(key: String, value: Int): SharedPreferences.Editor {
            pending[key] = value
            return this
        }

        /**
         * Schedules a long write for [key].
         * @return this editor, for chaining.
         */
        override fun putLong(key: String, value: Long): SharedPreferences.Editor {
            pending[key] = value
            return this
        }

        /**
         * Schedules a float write for [key].
         * @return this editor, for chaining.
         */
        override fun putFloat(key: String, value: Float): SharedPreferences.Editor {
            pending[key] = value
            return this
        }

        /**
         * Schedules a boolean write for [key].
         * @return this editor, for chaining.
         */
        override fun putBoolean(key: String, value: Boolean): SharedPreferences.Editor {
            pending[key] = value
            return this
        }

        /**
         * Schedules the removal of [key]. Takes effect on [commit] or [apply].
         * @return this editor, for chaining.
         */
        override fun remove(key: String): SharedPreferences.Editor {
            pending[key] = REMOVE
            return this
        }

        /**
         * Schedules a full store clear. Executed before any pending puts on [commit] or [apply].
         * @return this editor, for chaining.
         */
        override fun clear(): SharedPreferences.Editor {
            clearRequested = true
            return this
        }

        /**
         * Flushes all pending operations synchronously.
         * Returns true always — QuickStore has no async write queue.
         *
         * Per SharedPreferences contract: clear is applied BEFORE puts within the same editor.
         */
        override fun commit(): Boolean {
            flush()
            return true
        }

        /**
         * Flushes all pending operations synchronously.
         * QuickStore has no async write queue, so apply() behaves identically to commit().
         */
        override fun apply() {
            flush()
        }

        /**
         * Not supported — throws [UnsupportedOperationException].
         *
         * The QuickStore C ABI has no multi-value encoding.
         * @throws UnsupportedOperationException always.
         */
        override fun putStringSet(key: String, values: MutableSet<String>?): SharedPreferences.Editor =
            throw UnsupportedOperationException("Not supported by QuickStore")

        private fun flush() {
            // clear-first per SharedPreferences contract
            if (clearRequested) {
                store.clear()
            }

            for ((k, v) in pending) {
                @Suppress("UNCHECKED_CAST")
                when {
                    v === REMOVE    -> store.remove(k)
                    v == null       -> store.remove(k)   // putString(k, null) == remove per SP contract
                    v is String     -> store.putString(k, v)
                    v is Int        -> store.putInt(k, v)
                    v is Long       -> store.setLong(k, v)
                    v is Float      -> store.putFloat(k, v)
                    v is Boolean    -> store.setBool(k, v)
                }
            }

            pending.clear()
            clearRequested = false
        }
    }
}
