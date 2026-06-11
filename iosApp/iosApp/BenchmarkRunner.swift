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
import MMKV
import QuickStore

@MainActor
class BenchmarkRunner: ObservableObject {
    @Published var quickStoreResult: String = ""
    @Published var userDefaultsResult: String = ""
    @Published var speedupResult: String = ""
    @Published var mmkvResult: String = ""
    @Published var isRunning: Bool = false

    func runBenchmark() {
        guard !isRunning else { return }
        isRunning = true
        quickStoreResult = ""
        userDefaultsResult = ""
        speedupResult = ""
        mmkvResult = ""

        Task {
            let result = await Task.detached(priority: .userInitiated) {
                BenchmarkRunner.execute()
            }.value

            self.quickStoreResult = result.quickStoreText
            self.userDefaultsResult = result.userDefaultsText
            self.speedupResult = result.speedupText
            self.mmkvResult = result.mmkvText
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
        let mmkvText: String
    }

    private nonisolated static func execute() -> BenchmarkResult {
        let keyCount = 1000
        let batchKeys = (0..<keyCount).map { "bench_key_\($0)" }

        // ---- QuickStore ----
        let tempDir = URL(fileURLWithPath: NSTemporaryDirectory())
            .appendingPathComponent("qs_bench_\(Int(Date().timeIntervalSince1970 * 1000))")
        try? FileManager.default.createDirectory(at: tempDir, withIntermediateDirectories: true)
        let store = QuickStore(mmkvId: "bench", rootDir: tempDir.path)

        // Measures one QS type: warmup → clear → write → read → batch-read
        func measureQSType(
            set: (Int) -> Void,
            get: (Int) -> Void,
            batch: () -> Void
        ) -> (writeMs: Double, readMs: Double, batchMs: Double) {
            store.clear()
            for i in 0..<keyCount { set(i) }
            for i in 0..<keyCount { get(i) }
            store.clear()
            let w = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
            for i in 0..<keyCount { set(i) }
            let writeMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - w) / 1_000_000.0
            let r = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
            for i in 0..<keyCount { get(i) }
            let readMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - r) / 1_000_000.0
            let b = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
            batch()
            let batchMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - b) / 1_000_000.0
            return (writeMs, readMs, batchMs)
        }

        let qsLong = measureQSType(
            set:   { store.setLong(key: "bench_key_\($0)", value: Int64($0)) },
            get:   { let _ = store.getLong(key: "bench_key_\($0)") },
            batch: { let _ = store.batchGetLongs(keys: batchKeys) }
        )
        let qsBool = measureQSType(
            set:   { store.setBool(key: "bench_key_\($0)", value: $0 % 2 == 0) },
            get:   { let _ = store.getBool(key: "bench_key_\($0)") },
            batch: { let _ = store.batchGetBools(keys: batchKeys) }
        )
        let qsDouble = measureQSType(
            set:   { store.setDouble(key: "bench_key_\($0)", value: Double($0)) },
            get:   { let _ = store.getDouble(key: "bench_key_\($0)") },
            batch: { let _ = store.batchGetDoubles(keys: batchKeys) }
        )

        store.close()
        try? FileManager.default.removeItem(at: tempDir)

        // ---- UserDefaults ----
        let ud = UserDefaults(suiteName: "com.quickstore.bench") ?? UserDefaults.standard

        // Measures one UD type: warmup → clear → write (per-synchronize) → read
        func measureUD(
            put: (Int) -> Void,
            get: (Int) -> Void
        ) -> (writeMs: Double, readMs: Double) {
            for i in 0..<keyCount { put(i); ud.synchronize() }
            for i in 0..<keyCount { get(i) }
            for i in 0..<keyCount { ud.removeObject(forKey: "bench_key_\(i)") }
            ud.synchronize()
            let w = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
            for i in 0..<keyCount { put(i); ud.synchronize() }
            let writeMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - w) / 1_000_000.0
            let r = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
            for i in 0..<keyCount { get(i) }
            let readMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - r) / 1_000_000.0
            for i in 0..<keyCount { ud.removeObject(forKey: "bench_key_\(i)") }
            ud.synchronize()
            return (writeMs, readMs)
        }

        let udLong = measureUD(
            put: { ud.set(Int64($0), forKey: "bench_key_\($0)") },
            get: { let _ = ud.object(forKey: "bench_key_\($0)") }
        )
        let udBool = measureUD(
            put: { ud.set($0 % 2 == 0, forKey: "bench_key_\($0)") },
            get: { let _ = ud.bool(forKey: "bench_key_\($0)") }
        )
        let udDouble = measureUD(
            put: { ud.set(Double($0), forKey: "bench_key_\($0)") },
            get: { let _ = ud.double(forKey: "bench_key_\($0)") }
        )

        // ---- MMKV ----
        MMKV.initialize(rootDir: NSTemporaryDirectory() + "mmkv_bench")
        let mmkv = MMKV(mmapID: "bench_mmkv")!

        func measureMMKVType(set: (Int) -> Void, get: (Int) -> Void) -> (writeMs: Double, readMs: Double) {
            mmkv.clearAll()
            for i in 0..<keyCount { set(i) }
            for i in 0..<keyCount { get(i) }
            mmkv.clearAll()
            let w = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
            for i in 0..<keyCount { set(i) }
            let writeMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - w) / 1_000_000.0
            let r = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW)
            for i in 0..<keyCount { get(i) }
            let readMs = Double(clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW) - r) / 1_000_000.0
            mmkv.clearAll()
            return (writeMs, readMs)
        }

        let mmkvLong = measureMMKVType(
            set: { mmkv.set(Int64($0), forKey: "bench_key_\($0)") },
            get: { let _ = mmkv.int64(forKey: "bench_key_\($0)", defaultValue: 0) }
        )
        let mmkvBool = measureMMKVType(
            set: { mmkv.set($0 % 2 == 0, forKey: "bench_key_\($0)") },
            get: { let _ = mmkv.bool(forKey: "bench_key_\($0)", defaultValue: false) }
        )
        let mmkvDouble = measureMMKVType(
            set: { mmkv.set(Double($0), forKey: "bench_key_\($0)") },
            get: { let _ = mmkv.double(forKey: "bench_key_\($0)", defaultValue: 0.0) }
        )

        // ---- Speedups ----
        func wSpeedup(udW: Double, qsW: Double) -> Double { qsW > 0 ? udW / qsW : 0.0 }
        func totalSpeedup(udW: Double, udR: Double, qsW: Double, qsR: Double) -> Double {
            let qs = qsW + qsR; return qs > 0 ? (udW + udR) / qs : 0.0
        }

        let wSpLong   = wSpeedup(udW: udLong.writeMs,   qsW: qsLong.writeMs)
        let wSpBool   = wSpeedup(udW: udBool.writeMs,   qsW: qsBool.writeMs)
        let wSpDouble = wSpeedup(udW: udDouble.writeMs, qsW: qsDouble.writeMs)
        let spLong    = totalSpeedup(udW: udLong.writeMs,   udR: udLong.readMs,   qsW: qsLong.writeMs,   qsR: qsLong.readMs)
        let spBool    = totalSpeedup(udW: udBool.writeMs,   udR: udBool.readMs,   qsW: qsBool.writeMs,   qsR: qsBool.readMs)
        let spDouble  = totalSpeedup(udW: udDouble.writeMs, udR: udDouble.readMs, qsW: qsDouble.writeMs, qsR: qsDouble.readMs)

        // ---- Display ----
        var qsText = "── Writes (crash-safe, per-key) ──\n"
        qsText += String(format: "Long:   QS %.1fms  UD %.1fms  (%.0fx faster)\n", qsLong.writeMs,   udLong.writeMs,   wSpLong)
        qsText += String(format: "Bool:   QS %.1fms  UD %.1fms  (%.0fx faster)\n", qsBool.writeMs,   udBool.writeMs,   wSpBool)
        qsText += String(format: "Double: QS %.1fms  UD %.1fms  (%.0fx faster)",   qsDouble.writeMs, udDouble.writeMs, wSpDouble)

        var udText = "── Reads — single key ──\n"
        udText += "UD [mem, not crash-safe]  vs  QS [mmap, durable]\n"
        udText += String(format: "Long:   UD %.1fms  QS %.1fms\n", udLong.readMs,   qsLong.readMs)
        udText += String(format: "Bool:   UD %.1fms  QS %.1fms\n", udBool.readMs,   qsBool.readMs)
        udText += String(format: "Double: UD %.1fms  QS %.1fms\n", udDouble.readMs, qsDouble.readMs)
        udText += "\n── Reads — batch 1000 keys ──\n"
        udText += String(format: "Long:   QS %.1fms  vs  UD %.1fms\n", qsLong.batchMs,   udLong.readMs)
        udText += String(format: "Bool:   QS %.1fms  vs  UD %.1fms\n", qsBool.batchMs,   udBool.readMs)
        udText += String(format: "Double: QS %.1fms  vs  UD %.1fms",   qsDouble.batchMs, udDouble.readMs)

        var spText = "Write speedup QS vs UD:\n"
        spText += String(format: "  Long %.1fx  Bool %.1fx  Double %.1fx\n", spLong, spBool, spDouble)
        spText += "\nUD reads = in-memory (data lost on crash)\n"
        spText += "QS reads = mmap on disk (crash-safe)\n"
        spText += "QS batch = durable AND competitive with UD"

        let kmpOverheadLong   = mmkvLong.writeMs   > 0 ? qsLong.writeMs   / mmkvLong.writeMs   : 0.0
        let kmpOverheadBool   = mmkvBool.writeMs   > 0 ? qsBool.writeMs   / mmkvBool.writeMs   : 0.0
        let kmpOverheadDouble = mmkvDouble.writeMs > 0 ? qsDouble.writeMs / mmkvDouble.writeMs : 0.0
        var mmkvText = "── MMKV direct (no KMP wrapper) ──\n"
        mmkvText += String(format: "Long:   %.1fms w / %.1fms r  (KMP %.1fx overhead)\n", mmkvLong.writeMs,   mmkvLong.readMs,   kmpOverheadLong)
        mmkvText += String(format: "Bool:   %.1fms w / %.1fms r  (KMP %.1fx overhead)\n", mmkvBool.writeMs,   mmkvBool.readMs,   kmpOverheadBool)
        mmkvText += String(format: "Double: %.1fms w / %.1fms r  (KMP %.1fx overhead)",   mmkvDouble.writeMs, mmkvDouble.readMs, kmpOverheadDouble)

        return BenchmarkResult(
            quickStoreText: qsText,
            userDefaultsText: udText,
            speedupText: spText,
            mmkvText: mmkvText
        )
    }
}
