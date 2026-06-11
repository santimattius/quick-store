package com.quickstore.sample

import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private val viewModel: BenchmarkViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val btnRun = findViewById<Button>(R.id.btnRun)
        val tvQuickStore = findViewById<TextView>(R.id.tvQuickStore)
        val tvSharedPrefs = findViewById<TextView>(R.id.tvSharedPrefs)
        val tvSpeedup = findViewById<TextView>(R.id.tvSpeedup)

        btnRun.setOnClickListener {
            viewModel.runBenchmark()
        }

        lifecycleScope.launch {
            viewModel.running.collect { running ->
                btnRun.isEnabled = !running
                if (running) {
                    tvQuickStore.text = "Writes: running…"
                    tvSharedPrefs.text = "Reads: running…"
                    tvSpeedup.text = "…"
                }
            }
        }

        lifecycleScope.launch {
            viewModel.result.collect { result ->
                if (result != null) {
                    // Write-only speedups (SP write / QS write per type)
                    val wSpeedupLong   = if (result.qsLongWriteMs   > 0) result.spLongWriteMs   / result.qsLongWriteMs   else 0.0
                    val wSpeedupBool   = if (result.qsBoolWriteMs   > 0) result.spBoolWriteMs   / result.qsBoolWriteMs   else 0.0
                    val wSpeedupDouble = if (result.qsDoubleWriteMs > 0) result.spDoubleWriteMs / result.qsDoubleWriteMs else 0.0

                    val kmpOverheadLong   = if (result.mmkvLongWriteMs   > 0) result.qsLongWriteMs   / result.mmkvLongWriteMs   else 0.0
                    val kmpOverheadBool   = if (result.mmkvBoolWriteMs   > 0) result.qsBoolWriteMs   / result.mmkvBoolWriteMs   else 0.0
                    val kmpOverheadDouble = if (result.mmkvDoubleWriteMs > 0) result.qsDoubleWriteMs / result.mmkvDoubleWriteMs else 0.0

                    tvQuickStore.text = buildString {
                        append("── Writes (crash-safe, per-key) ──\n")
                        append("Long:   QS ${"%.1f".format(result.qsLongWriteMs)}ms  SP ${"%.1f".format(result.spLongWriteMs)}ms  (${"%.0f".format(wSpeedupLong)}x faster)\n")
                        append("Bool:   QS ${"%.1f".format(result.qsBoolWriteMs)}ms  SP ${"%.1f".format(result.spBoolWriteMs)}ms  (${"%.0f".format(wSpeedupBool)}x faster)\n")
                        append("Double: QS ${"%.1f".format(result.qsDoubleWriteMs)}ms  SP ${"%.1f".format(result.spDoubleWriteMs)}ms  (${"%.0f".format(wSpeedupDouble)}x faster)\n")
                        append("\nMMKV Long:   ${"%.1f".format(result.mmkvLongWriteMs)}ms w / ${"%.1f".format(result.mmkvLongReadMs)}ms r   (KMP ${"%.1f".format(kmpOverheadLong)}x overhead)\n")
                        append("MMKV Bool:   ${"%.1f".format(result.mmkvBoolWriteMs)}ms w / ${"%.1f".format(result.mmkvBoolReadMs)}ms r   (KMP ${"%.1f".format(kmpOverheadBool)}x overhead)\n")
                        append("MMKV Double: ${"%.1f".format(result.mmkvDoubleWriteMs)}ms w / ${"%.1f".format(result.mmkvDoubleReadMs)}ms r   (KMP ${"%.1f".format(kmpOverheadDouble)}x overhead)")
                    }
                    tvSharedPrefs.text = buildString {
                        append("── Reads — single key ──\n")
                        append("SP [heap, not crash-safe]  vs  QS [mmap+JNI, durable]\n")
                        append("Long:   SP ${"%.1f".format(result.spLongReadMs)}ms  QS ${"%.1f".format(result.qsLongReadMs)}ms\n")
                        append("Bool:   SP ${"%.1f".format(result.spBoolReadMs)}ms  QS ${"%.1f".format(result.qsBoolReadMs)}ms\n")
                        append("Double: SP ${"%.1f".format(result.spDoubleReadMs)}ms  QS ${"%.1f".format(result.qsDoubleReadMs)}ms\n")
                        append("\n")
                        append("── Reads — batch 1000 keys (JNI amortized) ──\n")
                        append("Long:   QS ${"%.1f".format(result.qsLongBatchReadMs)}ms  vs  SP ${"%.1f".format(result.spLongReadMs)}ms\n")
                        append("Bool:   QS ${"%.1f".format(result.qsBoolBatchReadMs)}ms  vs  SP ${"%.1f".format(result.spBoolReadMs)}ms\n")
                        append("Double: QS ${"%.1f".format(result.qsDoubleBatchReadMs)}ms  vs  SP ${"%.1f".format(result.spDoubleReadMs)}ms\n")
                        append("\nMMKV Long:   ${"%.1f".format(result.mmkvLongReadMs)}ms r\n")
                        append("MMKV Bool:   ${"%.1f".format(result.mmkvBoolReadMs)}ms r\n")
                        append("MMKV Double: ${"%.1f".format(result.mmkvDoubleReadMs)}ms r")
                    }
                    tvSpeedup.text = buildString {
                        append("Write speedup QS vs SP:\n")
                        append("  Long ${"%.1f".format(result.speedupLong)}x  Bool ${"%.1f".format(result.speedupBool)}x  Double ${"%.1f".format(result.speedupDouble)}x\n")
                        append("\n")
                        append("SP reads = JVM heap (data lost on crash)\n")
                        append("QS reads = mmap on disk (crash-safe)\n")
                        append("QS batch = durable AND competitive with SP\n")
                        append("\nKMP overhead (QS/MMKV write):\n")
                        append("  Long ${"%.1f".format(kmpOverheadLong)}x  Bool ${"%.1f".format(kmpOverheadBool)}x  Double ${"%.1f".format(kmpOverheadDouble)}x")
                    }
                }
            }
        }
    }
}
