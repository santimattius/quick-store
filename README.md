# QuickStore

A fast, MMKV-compatible key-value store for Kotlin Multiplatform — Android and iOS from a single API.

[![Maven Central](https://img.shields.io/maven-central/v/io.github.santimattius/quickstore.svg?label=Maven%20Central)](https://central.sonatype.com/artifact/io.github.santimattius/quickstore)
[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://www.apache.org/licenses/LICENSE-2.0)

---

## What is QuickStore?

QuickStore is a thin Kotlin Multiplatform wrapper over a C++ MMKV-compatible storage engine. It exposes a unified, type-safe API for persisting primitive values on Android (AAR + JNI) and iOS (XCFramework). The C++ core is frozen and battle-tested; QuickStore adds the Kotlin and Swift layers.

---

## Features

- Unified API across Android and iOS — one interface, two platforms
- MMKV-compatible binary storage format — fast reads and writes
- Type-safe getters that return `null` on miss (no sentinel values)
- Drop-in `SharedPreferences` adapter for Android migration
- Direct UserDefaults replacement on iOS
- No reflection, no annotation processors, no Kotlin Symbol Processing
- Fully synchronous — no coroutines required

---

## Requirements

| Platform | Minimum version |
|----------|----------------|
| Android  | minSdk 24 (Android 7.0) |
| iOS      | iOS 13+ |

---

## Installation

### Android

Add the dependency to your module's `build.gradle.kts`:

```kotlin
dependencies {
    implementation("io.github.santimattius:quickstore:0.1.0-alpha01")
}
```

Make sure `mavenCentral()` is in your repository list.

### iOS (Swift Package Manager)

In Xcode: **File > Add Package Dependencies**, enter the URL:

```
https://github.com/santimattius/quick-store
```

Select the `QuickStore` product and add it to your target.

Alternatively, add to your `Package.swift`:

```swift
dependencies: [
    .package(url: "https://github.com/santimattius/quick-store", from: "0.1.0-alpha01")
],
targets: [
    .target(
        name: "YourTarget",
        dependencies: ["QuickStore"]
    )
]
```

---

## Quick Start

### Android

```kotlin
import com.quickstore.QuickStore
import com.quickstore.putString
import com.quickstore.getString

// Open a store — mmkvId is the store name, rootDir is the storage directory
val store = QuickStore(
    mmkvId = "app_prefs",
    rootDir = context.filesDir.absolutePath
)

// Write
store.putString("username", "santiago")
store.setLong("login_count", 42L)
store.setBool("onboarding_done", true)

// Read
val username: String? = store.getString("username")
val count: Long? = store.getLong("login_count")
val done: Boolean? = store.getBool("onboarding_done")

// Default-value overloads
val safeUsername: String = store.getString("username", "guest")
val safeCount: Long = store.getLong("login_count", 0L)

// Cleanup
store.close()
```

### iOS (Swift)

```swift
import QuickStore

// Open a store
let store = QuickStore(
    mmkvId: "app_prefs",
    rootDir: FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!.path
)

// Write
_ = store.setLong(key: "login_count", value: 42)
_ = store.setBool(key: "onboarding_done", value: true)

// Read (returns Optional)
let count: Int64? = store.getLong(key: "login_count")
let done: Bool? = store.getBool(key: "onboarding_done")

// Cleanup
store.close()
```

---

## Migration

### Android — from SharedPreferences

`QuickStoreSharedPreferences` is a drop-in adapter that implements the `SharedPreferences` interface backed by a `QuickStore` instance. Swap one line:

```kotlin
// Before
val prefs: SharedPreferences = context.getSharedPreferences("app_prefs", Context.MODE_PRIVATE)

// After — same interface, faster storage
val store = QuickStore(mmkvId = "app_prefs", rootDir = context.filesDir.absolutePath)
val prefs: SharedPreferences = QuickStoreSharedPreferences(store)
```

All existing `getString`, `getInt`, `getLong`, `getFloat`, `getBoolean`, `contains`, `edit()`, `commit()`, and `apply()` calls continue to work without change.

See [Known Limitations](#known-limitations) for unsupported operations.

### iOS — from UserDefaults

Replace `UserDefaults` calls with the equivalent QuickStore primitives:

```swift
// UserDefaults
UserDefaults.standard.set("santiago", forKey: "username")
let name = UserDefaults.standard.string(forKey: "username")

// QuickStore — equivalent
_ = store.putString(key: "username", value: "santiago")   // extension
let name: String? = store.getString(key: "username")       // extension
```

Numeric migration:

```swift
// UserDefaults
UserDefaults.standard.set(42, forKey: "count")

// QuickStore
_ = store.setLong(key: "count", value: 42)
let count = store.getLong(key: "count")  // Int64?
```

---

## API Reference

### Constructor

| Signature | Platform | Description |
|-----------|----------|-------------|
| `QuickStore(mmkvId: String, rootDir: String)` | Both | Opens (or creates) a store with the given ID in `rootDir`. Throws on failure. |

### Primitive setters — return `true` on success

| Method | Platform | Description |
|--------|----------|-------------|
| `setBool(key: String, value: Boolean): Boolean` | Both | Stores a boolean value. |
| `setLong(key: String, value: Long): Boolean` | Both | Stores a 64-bit integer. |
| `setDouble(key: String, value: Double): Boolean` | Both | Stores a 64-bit float. |
| `setBytes(key: String, value: ByteArray): Boolean` | Both | Stores a raw byte array. |

### Primitive getters — return `null` when key not found

| Method | Platform | Description |
|--------|----------|-------------|
| `getBool(key: String): Boolean?` | Both | Retrieves a boolean, or `null` if absent. |
| `getLong(key: String): Long?` | Both | Retrieves a 64-bit integer, or `null` if absent. |
| `getDouble(key: String): Double?` | Both | Retrieves a 64-bit float, or `null` if absent. |
| `getBytes(key: String): ByteArray?` | Both | Retrieves a byte array, or `null` if absent. |

### Extension functions (Kotlin, `commonMain`)

| Method | Description |
|--------|-------------|
| `putString(key: String, value: String): Boolean` | Stores a UTF-8 string (encoded as bytes). |
| `getString(key: String): String?` | Retrieves a UTF-8 string, or `null` if absent. |
| `getString(key: String, default: String): String` | Retrieves a string, falling back to `default`. |
| `putInt(key: String, value: Int): Boolean` | Stores an integer (promoted to Long). |
| `getInt(key: String): Int?` | Retrieves an integer, or `null` if absent. |
| `getInt(key: String, default: Int): Int` | Retrieves an integer, falling back to `default`. |
| `putFloat(key: String, value: Float): Boolean` | Stores a float (promoted to Double). |
| `getFloat(key: String): Float?` | Retrieves a float, or `null` if absent. |
| `getFloat(key: String, default: Float): Float` | Retrieves a float, falling back to `default`. |
| `getBool(key: String, default: Boolean): Boolean` | Retrieves a boolean, falling back to `default`. |
| `getLong(key: String, default: Long): Long` | Retrieves a Long, falling back to `default`. |
| `getDouble(key: String, default: Double): Double` | Retrieves a Double, falling back to `default`. |

### Utility methods

| Method | Description |
|--------|-------------|
| `contains(key: String): Boolean` | Returns `true` if the key exists in the store. |
| `remove(key: String): Boolean` | Removes the entry for the given key. Returns `true` on success. |
| `count(): Long` | Returns the total number of entries in the store. |
| `allKeys(): List<String>` | Returns all keys currently stored. |
| `trim()` | Reclaims unused storage space (equivalent to MMKV trim). |
| `clear()` | Removes all entries from the store. |
| `close()` | Closes the store and releases native resources. |

### Android adapter

| Class / Method | Description |
|----------------|-------------|
| `QuickStoreSharedPreferences(store: QuickStore)` | Wraps a `QuickStore` as a `SharedPreferences`. Drop-in replacement for `getSharedPreferences(...)`. |

---

## Known Limitations

- **No `StringSet` support**: `getStringSet` / `putStringSet` throw `UnsupportedOperationException`. The C ABI has no multi-value encoding. Use `putString` with JSON serialization if you need sets.
- **No change listeners**: `registerOnSharedPreferenceChangeListener` / `unregisterOnSharedPreferenceChangeListener` throw `UnsupportedOperationException`. The frozen C++ core has no notification hook.
- **`commit()` == `apply()`**: Both are synchronous. `commit()` always returns `true`. There is no deferred write queue.
- **`QuickStoreFactory` does not exist**: The library does not ship a factory object. Instantiate `QuickStore(mmkvId, rootDir)` directly.

---

## License

```
Copyright 2024 Santiago Mattiauda

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
