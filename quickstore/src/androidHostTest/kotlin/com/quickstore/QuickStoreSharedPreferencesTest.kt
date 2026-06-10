package com.quickstore

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class QuickStoreSharedPreferencesTest {

    private fun prefs() = QuickStoreSharedPreferences(QuickStore("test", ""))

    @Test fun getStringEmptyStoreReturnsDefault() {
        assertEquals("def", prefs().getString("k", "def"))
    }

    @Test fun putStringGetStringRoundTrip() {
        val p = prefs()
        p.edit().putString("k", "v").commit()
        assertEquals("v", p.getString("k", ""))
    }

    @Test fun putIntGetIntRoundTrip() {
        val p = prefs()
        p.edit().putInt("k", 42).commit()
        assertEquals(42, p.getInt("k", 0))
    }

    @Test fun putLongGetLongRoundTrip() {
        val p = prefs()
        p.edit().putLong("k", 100L).commit()
        assertEquals(100L, p.getLong("k", 0L))
    }

    @Test fun putFloatGetFloatRoundTrip() {
        val p = prefs()
        p.edit().putFloat("k", 1.5f).commit()
        assertEquals(1.5f, p.getFloat("k", 0f))
    }

    @Test fun putBooleanGetBooleanRoundTrip() {
        val p = prefs()
        p.edit().putBoolean("k", true).commit()
        assertTrue(p.getBoolean("k", false))
    }

    @Test fun commitReturnsTrue() {
        assertTrue(prefs().edit().commit())
    }

    @Test fun applyIsEquivalentToCommit() {
        val p = prefs()
        p.edit().putString("k", "v").apply()
        assertEquals("v", p.getString("k", ""))
    }

    @Test fun containsFalseBeforePutTrueAfter() {
        val p = prefs()
        assertFalse(p.contains("k"))
        p.edit().putString("k", "v").commit()
        assertTrue(p.contains("k"))
    }

    @Test fun removeRemovesKey() {
        val p = prefs()
        p.edit().putString("k", "v").commit()
        p.edit().remove("k").commit()
        assertFalse(p.contains("k"))
    }

    @Test fun clearRemovesAllKeys() {
        val p = prefs()
        p.edit().putString("a", "1").putString("b", "2").commit()
        p.edit().clear().commit()
        assertTrue(p.getAll().isEmpty())
    }

    @Test fun clearBeforePutsOrdering() {
        val p = prefs()
        p.edit().putString("x", "old").commit()
        p.edit().putString("x", "new").clear().putString("y", "w").commit()
        assertFalse(p.contains("x"))
        assertTrue(p.contains("y"))
        assertEquals("w", p.getString("y", ""))
    }

    @Test fun getAllReturnsAllKeys() {
        val p = prefs()
        p.edit().putString("a", "1").putInt("b", 2).commit()
        val all = p.getAll()
        assertTrue(all.containsKey("a"))
        assertTrue(all.containsKey("b"))
    }

    @Test fun getStringSetThrows() {
        assertFailsWith<UnsupportedOperationException> {
            prefs().getStringSet("k", null)
        }
    }

    @Test fun registerListenerThrows() {
        assertFailsWith<UnsupportedOperationException> {
            prefs().registerOnSharedPreferenceChangeListener { _, _ -> }
        }
    }

    @Test fun unregisterListenerThrows() {
        assertFailsWith<UnsupportedOperationException> {
            prefs().unregisterOnSharedPreferenceChangeListener { _, _ -> }
        }
    }
}
