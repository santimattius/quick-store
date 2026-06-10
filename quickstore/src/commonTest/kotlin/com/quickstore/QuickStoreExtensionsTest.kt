package com.quickstore

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue

class QuickStoreExtensionsTest {

    private fun store() = QuickStore("test", "")

    @Test fun putStringGetStringRoundTrip() {
        val s = store()
        s.putString("k", "hello")
        assertEquals("hello", s.getString("k"))
    }

    @Test fun getStringMissingKeyNullable() {
        assertNull(store().getString("missing"))
    }

    @Test fun getStringMissingKeyDefault() {
        assertEquals("", store().getString("missing", ""))
        assertEquals("fallback", store().getString("missing", "fallback"))
    }

    @Test fun putStringUtf8MultiByte() {
        val s = store()
        s.putString("k", "日本語")
        assertEquals("日本語", s.getString("k"))
    }

    @Test fun putIntGetIntRoundTrip() {
        val s = store()
        s.putInt("k", 42)
        assertEquals(42, s.getInt("k", 0))
    }

    @Test fun getIntMissingKeyDefault() {
        assertEquals(0, store().getInt("k", 0))
        assertEquals(99, store().getInt("k", 99))
    }

    @Test fun putIntMaxValueNoOverflow() {
        val s = store()
        s.putInt("k", Int.MAX_VALUE)
        assertEquals(Int.MAX_VALUE, s.getInt("k", 0))
    }

    @Test fun putFloatGetFloatRoundTrip() {
        val s = store()
        s.putFloat("k", 3.14f)
        val result = s.getFloat("k", 0f)
        assertTrue(kotlin.math.abs(result - 3.14f) < 0.001f)
    }

    @Test fun getFloatMissingKeyDefault() {
        assertEquals(0f, store().getFloat("k", 0f))
        assertEquals(1.5f, store().getFloat("k", 1.5f))
    }

    @Test fun getBoolDefaultOverload() {
        val s = store()
        assertEquals(false, s.getBool("k", false))
        s.setBool("k", true)
        assertEquals(true, s.getBool("k", false))
    }

    @Test fun getLongDefaultOverload() {
        val s = store()
        assertEquals(0L, s.getLong("k", 0L))
        s.setLong("k", 42L)
        assertEquals(42L, s.getLong("k", 0L))
    }

    @Test fun getDoubleDefaultOverload() {
        val s = store()
        assertEquals(0.0, s.getDouble("k", 0.0))
        s.setDouble("k", 3.14)
        assertTrue(kotlin.math.abs(s.getDouble("k", 0.0) - 3.14) < 0.001)
    }
}
