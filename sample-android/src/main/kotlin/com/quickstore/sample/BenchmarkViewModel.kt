package com.quickstore.sample

import android.app.Application
import android.content.Context
import android.content.SharedPreferences
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.quickstore.ExperimentalQuickStoreApi
import com.quickstore.QuickStore
import com.tencent.mmkv.MMKV
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

data class BenchmarkResult(
    // Long
    val qsLongWriteMs: Double,
    val qsLongReadMs: Double,
    val qsLongBatchReadMs: Double,
    val spLongWriteMs: Double,
    val spLongReadMs: Double,
    val speedupLong: Double,
    // Bool
    val qsBoolWriteMs: Double,
    val qsBoolReadMs: Double,
    val qsBoolBatchReadMs: Double,
    val spBoolWriteMs: Double,
    val spBoolReadMs: Double,
    val speedupBool: Double,
    // Double
    val qsDoubleWriteMs: Double,
    val qsDoubleReadMs: Double,
    val qsDoubleBatchReadMs: Double,
    val spDoubleWriteMs: Double,
    val spDoubleReadMs: Double,
    val speedupDouble: Double,
    // MMKV
    val mmkvLongWriteMs: Double, val mmkvLongReadMs: Double,
    val mmkvBoolWriteMs: Double, val mmkvBoolReadMs: Double,
    val mmkvDoubleWriteMs: Double, val mmkvDoubleReadMs: Double,
)

class BenchmarkViewModel(application: Application) : AndroidViewModel(application) {

    private val _result = MutableStateFlow<BenchmarkResult?>(null)
    val result: StateFlow<BenchmarkResult?> = _result

    private val _running = MutableStateFlow(false)
    val running: StateFlow<Boolean> = _running

    fun runBenchmark() {
        if (_running.value) return
        viewModelScope.launch(Dispatchers.Default) {
            _running.value = true
            _result.value = null

            val context = getApplication<Application>()
            val benchResult = executeBenchmark(context)

            _result.value = benchResult
            _running.value = false
        }
    }

    private data class SpResult(val writeMs: Double, val readMs: Double)

    private fun measureSpType(
        sp: SharedPreferences,
        putKey: (Int) -> Unit,
        getKey: (Int) -> Unit
    ): SpResult {
        // Warmup — discarded
        repeat(1000) { putKey(it) }
        repeat(1000) { getKey(it) }
        sp.edit().clear().commit()
        // Measured write
        val w = System.nanoTime()
        repeat(1000) { putKey(it) }
        val writeMs = (System.nanoTime() - w) / 1_000_000.0
        // Measured read
        val r = System.nanoTime()
        repeat(1000) { getKey(it) }
        val readMs = (System.nanoTime() - r) / 1_000_000.0
        sp.edit().clear().commit()
        return SpResult(writeMs, readMs)
    }

    private fun measureMmkvType(
        mmkv: MMKV,
        encode: (Int) -> Unit,
        decode: (Int) -> Unit
    ): Pair<Double, Double> {
        mmkv.clearAll()
        repeat(1000) { encode(it) }
        repeat(1000) { decode(it) }
        mmkv.clearAll()
        val w = System.nanoTime()
        repeat(1000) { encode(it) }
        val writeMs = (System.nanoTime() - w) / 1_000_000.0
        val r = System.nanoTime()
        repeat(1000) { decode(it) }
        val readMs = (System.nanoTime() - r) / 1_000_000.0
        mmkv.clearAll()
        return Pair(writeMs, readMs)
    }

    private fun speedup(spW: Double, spR: Double, qsW: Double, qsR: Double): Double {
        val qs = qsW + qsR
        return if (qs > 0) (spW + spR) / qs else 0.0
    }

    @OptIn(ExperimentalQuickStoreApi::class)
    private fun executeBenchmark(context: Context): BenchmarkResult {
        // ---- QuickStore ----
        val tempDir = context.cacheDir.resolve("qs_bench_${System.currentTimeMillis()}")
        tempDir.mkdirs()
        val store = QuickStore("benchmark", tempDir.absolutePath)

        val batchKeys = (0 until 1000).map { "bench_key_$it" }

        // Warmup: absorbs JIT / first-page-fault skew — result discarded
        repeat(1000) { i -> store.setLong("bench_key_$i", i.toLong()) }
        repeat(1000) { i -> store.getLong("bench_key_$i") }
        store.batchGetLongs(batchKeys)   // warms up batch JNI + chunked/toTypedArray paths
        store.clear()

        // Measured write
        val qsLongWriteStart = System.nanoTime()
        repeat(1000) { i -> store.setLong("bench_key_$i", i.toLong()) }
        val qsLongWriteMs = (System.nanoTime() - qsLongWriteStart) / 1_000_000.0

        // Measured read — single-key path
        val qsLongReadStart = System.nanoTime()
        repeat(1000) { i -> store.getLong("bench_key_$i") }
        val qsLongReadMs = (System.nanoTime() - qsLongReadStart) / 1_000_000.0

        // Measured batch read — 1000 keys in a single batchGetLongs call
        val qsLongBatchReadStart = System.nanoTime()
        store.batchGetLongs(batchKeys)
        val qsLongBatchReadMs = (System.nanoTime() - qsLongBatchReadStart) / 1_000_000.0

        // ---- Bool ----
        store.clear()
        repeat(1000) { i -> store.setBool("bench_key_$i", i % 2 == 0) }
        repeat(1000) { i -> store.getBool("bench_key_$i") }
        store.clear()
        val qsBoolWriteStart = System.nanoTime()
        repeat(1000) { i -> store.setBool("bench_key_$i", i % 2 == 0) }
        val qsBoolWriteMs = (System.nanoTime() - qsBoolWriteStart) / 1_000_000.0
        val qsBoolReadStart = System.nanoTime()
        repeat(1000) { i -> store.getBool("bench_key_$i") }
        val qsBoolReadMs = (System.nanoTime() - qsBoolReadStart) / 1_000_000.0
        val qsBoolBatchStart = System.nanoTime()
        store.batchGetBools(batchKeys)
        val qsBoolBatchReadMs = (System.nanoTime() - qsBoolBatchStart) / 1_000_000.0

        // ---- Double ----
        store.clear()
        repeat(1000) { i -> store.setDouble("bench_key_$i", i.toDouble()) }
        repeat(1000) { i -> store.getDouble("bench_key_$i") }
        store.clear()
        val qsDoubleWriteStart = System.nanoTime()
        repeat(1000) { i -> store.setDouble("bench_key_$i", i.toDouble()) }
        val qsDoubleWriteMs = (System.nanoTime() - qsDoubleWriteStart) / 1_000_000.0
        val qsDoubleReadStart = System.nanoTime()
        repeat(1000) { i -> store.getDouble("bench_key_$i") }
        val qsDoubleReadMs = (System.nanoTime() - qsDoubleReadStart) / 1_000_000.0
        val qsDoubleBatchStart = System.nanoTime()
        store.batchGetDoubles(batchKeys)
        val qsDoubleBatchReadMs = (System.nanoTime() - qsDoubleBatchStart) / 1_000_000.0

        store.close()
        tempDir.deleteRecursively()

        // ---- SharedPreferences ----
        val sp = context.getSharedPreferences("qs_bench_sp", Context.MODE_PRIVATE)
        sp.edit().clear().commit()

        // SP Long
        val spLong = measureSpType(
            sp,
            putKey = { i -> sp.edit().putLong("bench_key_$i", i.toLong()).commit() },
            getKey = { i -> sp.getLong("bench_key_$i", 0L) }
        )

        // SP Bool
        val spBool = measureSpType(
            sp,
            putKey = { i -> sp.edit().putBoolean("bench_key_$i", i % 2 == 0).commit() },
            getKey = { i -> sp.getBoolean("bench_key_$i", false) }
        )

        // SP Double (Float-backed — SP has no putDouble)
        val spDouble = measureSpType(
            sp,
            putKey = { i -> sp.edit().putFloat("bench_key_$i", i.toDouble().toFloat()).commit() },
            getKey = { i -> sp.getFloat("bench_key_$i", 0f) }
        )

        val speedupLong = speedup(spLong.writeMs, spLong.readMs, qsLongWriteMs, qsLongReadMs)
        val speedupBool = speedup(spBool.writeMs, spBool.readMs, qsBoolWriteMs, qsBoolReadMs)
        val speedupDouble = speedup(spDouble.writeMs, spDouble.readMs, qsDoubleWriteMs, qsDoubleReadMs)

        // ---- MMKV ----
        MMKV.initialize(context)
        val mmkv = MMKV.mmkvWithID("bench_mmkv")

        val mmkvLong = measureMmkvType(
            mmkv,
            encode = { i -> mmkv.encode("bench_key_$i", i.toLong()) },
            decode = { i -> mmkv.decodeLong("bench_key_$i", 0L) }
        )
        val mmkvBool = measureMmkvType(
            mmkv,
            encode = { i -> mmkv.encode("bench_key_$i", i % 2 == 0) },
            decode = { i -> mmkv.decodeBool("bench_key_$i", false) }
        )
        val mmkvDouble = measureMmkvType(
            mmkv,
            encode = { i -> mmkv.encode("bench_key_$i", i.toDouble()) },
            decode = { i -> mmkv.decodeDouble("bench_key_$i", 0.0) }
        )

        return BenchmarkResult(
            qsLongWriteMs = qsLongWriteMs,
            qsLongReadMs = qsLongReadMs,
            qsLongBatchReadMs = qsLongBatchReadMs,
            spLongWriteMs = spLong.writeMs,
            spLongReadMs = spLong.readMs,
            speedupLong = speedupLong,
            qsBoolWriteMs = qsBoolWriteMs,
            qsBoolReadMs = qsBoolReadMs,
            qsBoolBatchReadMs = qsBoolBatchReadMs,
            spBoolWriteMs = spBool.writeMs,
            spBoolReadMs = spBool.readMs,
            speedupBool = speedupBool,
            qsDoubleWriteMs = qsDoubleWriteMs,
            qsDoubleReadMs = qsDoubleReadMs,
            qsDoubleBatchReadMs = qsDoubleBatchReadMs,
            spDoubleWriteMs = spDouble.writeMs,
            spDoubleReadMs = spDouble.readMs,
            speedupDouble = speedupDouble,
            mmkvLongWriteMs = mmkvLong.first,
            mmkvLongReadMs = mmkvLong.second,
            mmkvBoolWriteMs = mmkvBool.first,
            mmkvBoolReadMs = mmkvBool.second,
            mmkvDoubleWriteMs = mmkvDouble.first,
            mmkvDoubleReadMs = mmkvDouble.second,
        )
    }
}
