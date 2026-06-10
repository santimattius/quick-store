package com.quickstore.sample

import android.app.Application
import android.content.Context
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.quickstore.QuickStore
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

data class BenchmarkResult(
    val qsWriteMs: Long,
    val qsReadMs: Long,
    val spWriteMs: Long,
    val spReadMs: Long,
    val speedup: Double
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

    private fun executeBenchmark(context: Context): BenchmarkResult {
        // ---- QuickStore ----
        val tempDir = context.cacheDir.resolve("qs_bench_${System.currentTimeMillis()}")
        tempDir.mkdirs()
        val store = QuickStore("benchmark", tempDir.absolutePath)

        // Warmup: absorbs JIT / first-page-fault skew — result discarded
        repeat(1000) { i -> store.setLong("bench_key_$i", i.toLong()) }
        repeat(1000) { i -> store.getLong("bench_key_$i") }
        store.clear()

        // Measured write
        val qsWriteStart = System.nanoTime()
        repeat(1000) { i -> store.setLong("bench_key_$i", i.toLong()) }
        val qsWriteMs = (System.nanoTime() - qsWriteStart) / 1_000_000L

        // Measured read
        val qsReadStart = System.nanoTime()
        repeat(1000) { i -> store.getLong("bench_key_$i") }
        val qsReadMs = (System.nanoTime() - qsReadStart) / 1_000_000L

        store.close()
        tempDir.deleteRecursively()

        // ---- SharedPreferences ----
        val sp = context.getSharedPreferences("qs_bench_sp", Context.MODE_PRIVATE)
        sp.edit().clear().commit()

        // Warmup: absorbs SharedPreferences internal init skew — result discarded
        val warmupEditor = sp.edit()
        repeat(1000) { i -> warmupEditor.putLong("bench_key_$i", i.toLong()) }
        warmupEditor.commit()
        repeat(1000) { i -> sp.getLong("bench_key_$i", 0L) }
        sp.edit().clear().commit()

        // Measured write
        val spWriteStart = System.nanoTime()
        val spEditor = sp.edit()
        repeat(1000) { i -> spEditor.putLong("bench_key_$i", i.toLong()) }
        spEditor.commit()
        val spWriteMs = (System.nanoTime() - spWriteStart) / 1_000_000L

        // Measured read
        val spReadStart = System.nanoTime()
        repeat(1000) { i -> sp.getLong("bench_key_$i", 0L) }
        val spReadMs = (System.nanoTime() - spReadStart) / 1_000_000L

        sp.edit().clear().commit()

        val qsTotal = qsWriteMs + qsReadMs
        val spTotal = spWriteMs + spReadMs
        // Speedup: how many times faster QuickStore is vs SharedPreferences.
        // If qsTotal == 0, report 0x to avoid division by zero.
        val speedup = if (qsTotal > 0) spTotal.toDouble() / qsTotal.toDouble() else 0.0

        return BenchmarkResult(qsWriteMs, qsReadMs, spWriteMs, spReadMs, speedup)
    }
}
