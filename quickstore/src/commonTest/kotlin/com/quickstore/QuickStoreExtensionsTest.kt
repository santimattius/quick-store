package com.quickstore

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNull
import kotlin.test.assertTrue
import kotlin.test.assertFalse

class QuickStoreExtensionsTest {

    private fun store() = createTestStore()

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

    // SR-1: batchGetLongs — all four scenarios

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetLongsAllKeysPresent() {
        val s = store()
        s.setLong("a", 1L)
        s.setLong("b", 2L)
        val result = s.batchGetLongs(listOf("a", "b"))
        assertEquals(2, result.size)
        assertEquals(1L, result["a"])
        assertEquals(2L, result["b"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetLongsMixedPresentAndAbsent() {
        val s = store()
        s.setLong("x", 42L)
        val result = s.batchGetLongs(listOf("x", "y"))
        assertEquals(2, result.size)
        assertEquals(42L, result["x"])
        assertTrue(result.containsKey("y"))
        assertNull(result["y"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetLongsAllAbsent() {
        val s = store()
        val result = s.batchGetLongs(listOf("p", "q"))
        assertEquals(2, result.size)
        assertTrue(result.containsKey("p"))
        assertTrue(result.containsKey("q"))
        assertNull(result["p"])
        assertNull(result["q"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetLongsEmptyListReturnsEmptyMap() {
        val result = store().batchGetLongs(emptyList())
        assertTrue(result.isEmpty())
    }

    // SR-6: batchGetBools — all four scenarios

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetBoolsAllKeysPresent() {
        val s = store()
        s.setBool("a", true)
        s.setBool("b", false)
        val result = s.batchGetBools(listOf("a", "b"))
        assertEquals(2, result.size)
        assertEquals(true, result["a"])
        assertEquals(false, result["b"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetBoolsMixedPresentAndAbsent() {
        val s = store()
        s.setBool("x", true)
        val result = s.batchGetBools(listOf("x", "y"))
        assertEquals(2, result.size)
        assertEquals(true, result["x"])
        assertTrue(result.containsKey("y"))
        assertNull(result["y"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetBoolsAllAbsent() {
        val s = store()
        val result = s.batchGetBools(listOf("p", "q"))
        assertEquals(2, result.size)
        assertTrue(result.containsKey("p"))
        assertTrue(result.containsKey("q"))
        assertNull(result["p"])
        assertNull(result["q"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetBoolsEmptyListReturnsEmptyMap() {
        val result = store().batchGetBools(emptyList())
        assertTrue(result.isEmpty())
    }

    // SR-7: batchGetDoubles — all four scenarios

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetDoublesAllKeysPresent() {
        val s = store()
        s.setDouble("pi", 3.14)
        s.setDouble("e", 2.71)
        val result = s.batchGetDoubles(listOf("pi", "e"))
        assertEquals(2, result.size)
        assertTrue(kotlin.math.abs(result["pi"]!! - 3.14) < 0.001)
        assertTrue(kotlin.math.abs(result["e"]!! - 2.71) < 0.001)
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetDoublesMixedPresentAndAbsent() {
        val s = store()
        s.setDouble("x", 1.5)
        val result = s.batchGetDoubles(listOf("x", "y"))
        assertEquals(2, result.size)
        assertTrue(kotlin.math.abs(result["x"]!! - 1.5) < 0.001)
        assertTrue(result.containsKey("y"))
        assertNull(result["y"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetDoublesAllAbsent() {
        val s = store()
        val result = s.batchGetDoubles(listOf("p", "q"))
        assertEquals(2, result.size)
        assertTrue(result.containsKey("p"))
        assertTrue(result.containsKey("q"))
        assertNull(result["p"])
        assertNull(result["q"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchGetDoublesEmptyListReturnsEmptyMap() {
        val result = store().batchGetDoubles(emptyList())
        assertTrue(result.isEmpty())
    }

    // SR-W1..W4 + SR-W7: batchSetLongs

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetLongsRoundTripViaBatchGet() {
        val s = store()
        val pairs = mapOf("a" to 1L, "b" to 2L)
        s.batchSetLongs(pairs)
        val result = s.batchGetLongs(listOf("a", "b"))
        assertEquals(2, result.size)
        assertEquals(1L, result["a"])
        assertEquals(2L, result["b"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetLongsEmptyMapIsNoOp() {
        val s = store()
        val before = s.count()
        s.batchSetLongs(emptyMap())
        assertEquals(before, s.count())
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetLongsOverwritesExistingValues() {
        val s = store()
        s.batchSetLongs(mapOf("a" to 1L))
        s.batchSetLongs(mapOf("a" to 99L))
        assertEquals(99L, s.getLong("a"))
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetLongs1000KeysRoundTrip() {
        val s = store()
        val pairs = (0 until 1000).associate { "key$it" to it.toLong() }
        s.batchSetLongs(pairs)
        val result = s.batchGetLongs(pairs.keys.toList())
        assertEquals(1000, result.size)
        pairs.forEach { (k, v) -> assertEquals(v, result[k], "Mismatch for key $k") }
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetLongsThenSingleGetPerKey() {
        val s = store()
        val pairs = mapOf("x" to 10L, "y" to 20L, "z" to 30L)
        s.batchSetLongs(pairs)
        assertEquals(10L, s.getLong("x"))
        assertEquals(20L, s.getLong("y"))
        assertEquals(30L, s.getLong("z"))
    }

    // SR-W5 + SR-W7: batchSetBools

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetBoolsRoundTripViaBatchGet() {
        val s = store()
        val pairs = mapOf("a" to true, "b" to false)
        s.batchSetBools(pairs)
        val result = s.batchGetBools(listOf("a", "b"))
        assertEquals(2, result.size)
        assertEquals(true, result["a"])
        assertEquals(false, result["b"])
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetBoolsEmptyMapIsNoOp() {
        val s = store()
        val before = s.count()
        s.batchSetBools(emptyMap())
        assertEquals(before, s.count())
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetBoolsOverwritesExistingValues() {
        val s = store()
        s.batchSetBools(mapOf("flag" to true))
        s.batchSetBools(mapOf("flag" to false))
        assertEquals(false, s.getBool("flag"))
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetBools1000KeysRoundTrip() {
        val s = store()
        val pairs = (0 until 1000).associate { "key$it" to (it % 2 == 0) }
        s.batchSetBools(pairs)
        val result = s.batchGetBools(pairs.keys.toList())
        assertEquals(1000, result.size)
        pairs.forEach { (k, v) -> assertEquals(v, result[k], "Mismatch for key $k") }
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetBoolsThenSingleGetPerKey() {
        val s = store()
        val pairs = mapOf("x" to true, "y" to false, "z" to true)
        s.batchSetBools(pairs)
        assertEquals(true, s.getBool("x"))
        assertEquals(false, s.getBool("y"))
        assertEquals(true, s.getBool("z"))
    }

    // SR-W6 + SR-W7: batchSetDoubles

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetDoublesRoundTripViaBatchGet() {
        val s = store()
        val pairs = mapOf("pi" to 3.14, "e" to 2.71)
        s.batchSetDoubles(pairs)
        val result = s.batchGetDoubles(listOf("pi", "e"))
        assertEquals(2, result.size)
        assertTrue(kotlin.math.abs(result["pi"]!! - 3.14) < 0.001)
        assertTrue(kotlin.math.abs(result["e"]!! - 2.71) < 0.001)
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetDoublesEmptyMapIsNoOp() {
        val s = store()
        val before = s.count()
        s.batchSetDoubles(emptyMap())
        assertEquals(before, s.count())
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetDoublesOverwritesExistingValues() {
        val s = store()
        s.batchSetDoubles(mapOf("x" to 1.0))
        s.batchSetDoubles(mapOf("x" to 99.9))
        val actual = s.getDouble("x")!!
        assertTrue(kotlin.math.abs(actual - 99.9) < 0.001)
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetDoubles1000KeysRoundTrip() {
        val s = store()
        val pairs = (0 until 1000).associate { "key$it" to it.toDouble() * 0.5 }
        s.batchSetDoubles(pairs)
        val result = s.batchGetDoubles(pairs.keys.toList())
        assertEquals(1000, result.size)
        pairs.forEach { (k, v) ->
            assertTrue(kotlin.math.abs(result[k]!! - v) < 0.001, "Mismatch for key $k")
        }
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    @Test fun batchSetDoublesThenSingleGetPerKey() {
        val s = store()
        val pairs = mapOf("x" to 1.1, "y" to 2.2, "z" to 3.3)
        s.batchSetDoubles(pairs)
        assertTrue(kotlin.math.abs(s.getDouble("x")!! - 1.1) < 0.001)
        assertTrue(kotlin.math.abs(s.getDouble("y")!! - 2.2) < 0.001)
        assertTrue(kotlin.math.abs(s.getDouble("z")!! - 3.3) < 0.001)
    }
}
