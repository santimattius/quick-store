package com.quickstore.benchmark

import androidx.benchmark.junit4.BenchmarkRule
import androidx.benchmark.junit4.measureRepeated
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.quickstore.QuickStore
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@RunWith(AndroidJUnit4::class)
class QuickStoreBenchmark {

    @get:Rule
    val benchmarkRule = BenchmarkRule()

    private lateinit var store: QuickStore
    private lateinit var tempDir: File
    private val bytesValue = ByteArray(64) { it.toByte() }

    @Before
    fun setUp() {
        val ctx = InstrumentationRegistry.getInstrumentation().context
        tempDir = ctx.cacheDir.resolve("qs_bench_${System.currentTimeMillis()}")
        tempDir.mkdirs()
        store = QuickStore("benchmark", tempDir.absolutePath)

        store.setLong("readLong", 42L)
        store.setBool("readBool", true)
        store.setDouble("readDouble", 3.14159)
        store.setBytes("readBytes", bytesValue)
        store.setLong("containsKey", 1L)
        repeat(1000) { store.setLong("k$it", it.toLong()) }
    }

    @After
    fun tearDown() {
        store.close()
        tempDir.deleteRecursively()
    }

    @Test
    fun setLong() = benchmarkRule.measureRepeated {
        store.setLong("key", 42L)
    }

    @Test
    fun getLong() = benchmarkRule.measureRepeated {
        store.getLong("readLong")
    }

    @Test
    fun setBool() = benchmarkRule.measureRepeated {
        store.setBool("key", true)
    }

    @Test
    fun getBool() = benchmarkRule.measureRepeated {
        store.getBool("readBool")
    }

    @Test
    fun setDouble() = benchmarkRule.measureRepeated {
        store.setDouble("key", 3.14159)
    }

    @Test
    fun getDouble() = benchmarkRule.measureRepeated {
        store.getDouble("readDouble")
    }

    @Test
    fun setBytes() = benchmarkRule.measureRepeated {
        store.setBytes("key", bytesValue)
    }

    @Test
    fun getBytes() = benchmarkRule.measureRepeated {
        store.getBytes("readBytes")
    }

    @Test
    fun contains() = benchmarkRule.measureRepeated {
        store.contains("containsKey")
    }

    @Test
    fun remove() = benchmarkRule.measureRepeated {
        runWithMeasurementDisabled { store.setLong("removeKey", 1L) }
        store.remove("removeKey")
    }

    @Test
    fun allKeys() = benchmarkRule.measureRepeated {
        store.allKeys()
    }
}
