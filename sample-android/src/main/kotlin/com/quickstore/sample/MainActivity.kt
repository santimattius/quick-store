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
                    tvQuickStore.text = "QuickStore: running…"
                    tvSharedPrefs.text = "SharedPreferences: running…"
                    tvSpeedup.text = "Speedup: …"
                }
            }
        }

        lifecycleScope.launch {
            viewModel.result.collect { result ->
                if (result != null) {
                    tvQuickStore.text =
                        "QuickStore: ${result.qsWriteMs}ms write / ${result.qsReadMs}ms read"
                    tvSharedPrefs.text =
                        "SharedPreferences: ${result.spWriteMs}ms write / ${result.spReadMs}ms read"
                    val speedupFormatted = "%.2f".format(result.speedup)
                    tvSpeedup.text = "Speedup: ${speedupFormatted}x (QuickStore vs SP)"
                }
            }
        }
    }
}
