// =============================================================================
// QuickStore XCFramework — Integration Notes
// =============================================================================
//
// Authoritative XCFramework path:
//   quickstore/build/XCFrameworks/release/QuickStore.xcframework
//
// To consume in this Xcode project:
//   1. Run the Gradle task from the repo root:
//        ./gradlew :quickstore:assembleQuickStoreXCFramework
//   2. In Xcode → project settings → target → "Frameworks, Libraries, and
//      Embedded Content" → "+" → "Add Other…" → "Add Files…"
//      Navigate to quickstore/build/XCFrameworks/release/QuickStore.xcframework
//      and select "Do Not Embed".
//   3. Add `import QuickStore` to this file and remove the TODO stubs below.
//
// Kotlin extension functions in Swift (Obj-C name mangling):
//   KMP extension functions (QuickStoreExtensions.kt) are exposed as top-level
//   Obj-C functions on the generated header, callable from Swift as:
//     QuickStoreExtensionsKt.putString(store, key: "k", value: "v")
//     QuickStoreExtensionsKt.getString(store, key: "k", default_: "fallback")
//     QuickStoreExtensionsKt.putInt(store, key: "k", value: 42)
//     QuickStoreExtensionsKt.getInt(store, key: "k", default_: 0)
//   The `QuickStore` class itself is available directly as `QuickStore(mmkvId:rootDir:)`.
//
// =============================================================================

import Foundation
import QuickStore

@MainActor
class BenchmarkRunner: ObservableObject {
    @Published var quickStoreResult: String = ""
    @Published var userDefaultsResult: String = ""
    @Published var speedupResult: String = ""
    @Published var isRunning: Bool = false

    func runBenchmark() {
        guard !isRunning else { return }
        isRunning = true
        quickStoreResult = ""
        userDefaultsResult = ""
        speedupResult = ""

        Task {
            let result = await Task.detached(priority: .userInitiated) {
                BenchmarkRunner.execute()
            }.value

            self.quickStoreResult = result.quickStoreText
            self.userDefaultsResult = result.userDefaultsText
            self.speedupResult = result.speedupText
            self.isRunning = false
        }
    }

    // -------------------------------------------------------------------------
    // Benchmark logic (runs off main actor)
    // -------------------------------------------------------------------------

    private struct BenchmarkResult {
        let quickStoreText: String
        let userDefaultsText: String
        let speedupText: String
    }

    private nonisolated static func execute() -> BenchmarkResult {
        let keyCount = 1000

        // ---- QuickStore ----
        let tempDir = URL(fileURLWithPath: NSTemporaryDirectory())
            .appendingPathComponent("qs_bench_\(Int(Date().timeIntervalSince1970 * 1000))")
        try? FileManager.default.createDirectory(at: tempDir, withIntermediateDirectories: true)
        let store = QuickStore(mmkvId: "bench", rootDir: tempDir.path)

        // Warmup (discarded)
        for i in 0..<keyCount { store.setLong(key: "bench_key_\(i)", value: Int64(i)) }
        for i in 0..<keyCount { let _ = store.getLong(key: "bench_key_\(i)") }
        store.clear()

        // Measured write
        let qsWriteStart = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
        for i in 0..<keyCount { store.setLong(key: "bench_key_\(i)", value: Int64(i)) }
        let qsWriteMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - qsWriteStart) / 1_000_000.0

        // Measured read
        let qsReadStart = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
        for i in 0..<keyCount { let _ = store.getLong(key: "bench_key_\(i)") }
        let qsReadMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - qsReadStart) / 1_000_000.0

        store.close()
        try? FileManager.default.removeItem(at: tempDir)

        // ---- UserDefaults ----
        let ud = UserDefaults(suiteName: "com.quickstore.bench") ?? UserDefaults.standard

        // Warmup (discarded)
        for i in 0..<keyCount { ud.set(Int64(i), forKey: "bench_key_\(i)") }
        for i in 0..<keyCount { let _ = ud.object(forKey: "bench_key_\(i)") }
        for i in 0..<keyCount { ud.removeObject(forKey: "bench_key_\(i)") }
        ud.synchronize()

        // Measured write
        let udWriteStart = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
        for i in 0..<keyCount { ud.set(Int64(i), forKey: "bench_key_\(i)") }
        ud.synchronize()
        let udWriteMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - udWriteStart) / 1_000_000.0

        // Measured read
        let udReadStart = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
        for i in 0..<keyCount { let _ = ud.object(forKey: "bench_key_\(i)") }
        let udReadMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - udReadStart) / 1_000_000.0

        // Cleanup
        for i in 0..<keyCount { ud.removeObject(forKey: "bench_key_\(i)") }
        ud.synchronize()

        // ---- Results ----
        let udTotal = udWriteMs + udReadMs
        let udText = String(format: "write: %.1f ms / read: %.1f ms", udWriteMs, udReadMs)

        let qsTotal = qsWriteMs + qsReadMs
        let speedup = qsTotal > 0 ? udTotal / qsTotal : 0.0
        return BenchmarkResult(
            quickStoreText: String(format: "write: %.1f ms / read: %.1f ms", qsWriteMs, qsReadMs),
            userDefaultsText: udText,
            speedupText: String(format: "%.2fx (QuickStore vs UserDefaults)", speedup)
        )
    }
}
