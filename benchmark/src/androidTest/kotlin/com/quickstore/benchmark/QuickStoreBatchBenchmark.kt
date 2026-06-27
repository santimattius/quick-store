package com.quickstore.benchmark

import androidx.benchmark.junit4.BenchmarkRule
import androidx.benchmark.junit4.measureRepeated
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import com.quickstore.ExperimentalQuickStoreApi
import com.quickstore.QuickStore
import org.junit.After
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File

@OptIn(ExperimentalQuickStoreApi::class)
@RunWith(AndroidJUnit4::class)
class QuickStoreBatchBenchmark {

    @get:Rule
    val benchmarkRule = BenchmarkRule()

    private lateinit var store: QuickStore
    private lateinit var tempDir: File

    private val keys: List<String> = (0 until 1000).map { "key_$it" }
    private val longMap: Map<String, Long> = keys.associateWith { it.hashCode().toLong() }
    private val boolMap: Map<String, Boolean> = keys.associateWith { it.hashCode() % 2 == 0 }
    private val doubleMap: Map<String, Double> = keys.associateWith { it.hashCode().toDouble() }

    @Before
    fun setUp() {
        val ctx = InstrumentationRegistry.getInstrumentation().context
        tempDir = ctx.cacheDir.resolve("qs_batch_bench_${System.currentTimeMillis()}")
        tempDir.mkdirs()
        store = QuickStore("batch_benchmark", tempDir.absolutePath)

        store.batchSetLongs(longMap)
        store.batchSetBools(boolMap)
        store.batchSetDoubles(doubleMap)
    }

    @After
    fun tearDown() {
        store.close()
        tempDir.deleteRecursively()
    }

    @Test
    fun batchGetLongs() = benchmarkRule.measureRepeated {
        store.batchGetLongs(keys)
    }

    @Test
    fun batchGetBools() = benchmarkRule.measureRepeated {
        store.batchGetBools(keys)
    }

    @Test
    fun batchGetDoubles() = benchmarkRule.measureRepeated {
        store.batchGetDoubles(keys)
    }

    @Test
    fun batchSetLongs() = benchmarkRule.measureRepeated {
        store.batchSetLongs(longMap)
    }

    @Test
    fun batchSetBools() = benchmarkRule.measureRepeated {
        store.batchSetBools(boolMap)
    }

    @Test
    fun batchSetDoubles() = benchmarkRule.measureRepeated {
        store.batchSetDoubles(doubleMap)
    }
}
