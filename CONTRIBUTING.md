# Contributing to QuickStore

Thank you for your interest in contributing. QuickStore is a Kotlin Multiplatform wrapper over a frozen C++ MMKV-compatible core, shipping two Maven artifacts (`quickstore` + `quickstore-native`) and one XCFramework for iOS.

**Issue-first policy:** please open or comment on an issue before opening a large pull request. This avoids duplicated effort and ensures the change aligns with the project direction.

---

## Prerequisites

| Tool | Required version | Notes |
|------|-----------------|-------|
| JDK | 17 (Temurin recommended) | Matches the `publish.yml` CI environment |
| Android SDK | minSdk 24 | Set `ANDROID_HOME` or use Android Studio |
| CMake | ≥ 3.21 | Matches the root `CMakeLists.txt` requirement |
| Xcode + CLI tools | Latest stable | Required for XCFramework builds on macOS |
| zlib | System-provided | Used via `find_package(ZLIB)` on non-Android builds |

---

## Repository layout

```
quick-store/
├── core/                  # C++ MMKV-compatible core + GoogleTest suite
│   └── tests/             # C++ unit tests and golden-file tests
├── quickstore/            # Kotlin Multiplatform module
│   └── src/
│       ├── commonMain/    # expect declarations + extension functions
│       ├── androidMain/   # Android actual + SharedPreferences adapter
│       ├── iosMain/       # iOS/Native actual via cinterop
│       └── commonTest/    # Shared test helpers (FakeQuickStore, createTestStore)
├── quickstore-native/     # Android JNI AAR (libquickstore_jni.so)
└── Package.swift          # Swift Package Manager binary target (see §Release flow)
```

---

## Building the C++ core

Clone the repository first if you have not already:

```bash
git clone https://github.com/santimattius/quick-store.git
cd quick-store
```

Then build:

```bash
cmake -S . -B build && cmake --build build
```

The build defaults to `CMAKE_BUILD_TYPE=Debug`. To build Release:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

The core requires C++17; `CMAKE_CXX_EXTENSIONS` is explicitly set to `OFF` in `CMakeLists.txt`.

---

## Running C++ tests

The root CMake configuration calls `enable_testing()` and registers a GoogleTest suite under `core/tests/`. After building, run:

```bash
ctest --test-dir build --output-on-failure
```

This covers both unit tests and any golden-file tests in `core/tests/`.

---

## Running Kotlin / KMP tests

Run all Kotlin tests across platforms:

```bash
./gradlew :quickstore:allTests
```

To run Android-only unit tests:

```bash
./gradlew :quickstore:testDebugUnitTest
```

The shared test helpers live in `quickstore/src/commonTest/kotlin/com/quickstore/`. `FakeQuickStore` is the in-memory test double; `createTestStore()` is the `expect fun` used in platform-specific tests. See the [Architecture overview](README.md#architecture-overview) for details on `FakeQuickStore`.

---

## Building the XCFramework locally

```bash
./gradlew :quickstore:assembleQuickStoreReleaseXCFramework
```

Output: `quickstore/build/XCFrameworks/release/QuickStore.xcframework`

This is the exact task used by the release workflow.

---

## Release flow

Releases are fully automated and tag-driven. **Do NOT hand-edit `Package.swift` on `main`.**

1. Push a `v*.*.*` tag to `main`.
2. The `publish.yml` workflow triggers and:
   - Builds and zips the XCFramework.
   - Computes the checksum via `swift package compute-checksum`.
   - Replaces the `REPLACE_WITH_CHECKSUM` placeholder (and the release URL) in `Package.swift` using `sed`.
   - Commits the resolved `Package.swift` back to `main`.
   - Publishes both `io.github.santimattius:quickstore` and `io.github.santimattius:quickstore-native` to Maven Central at the same version.

The `REPLACE_WITH_CHECKSUM` placeholder you see on `main` is intentional — the workflow owns it. Always consume the library via a tagged version (`from: "x.y.z"`), never via `branch: "main"`.

---

## Pull request process

- **Branch from `main`** and keep your branch short-lived.
- **Conventional commit titles** (e.g. `feat:`, `fix:`, `docs:`, `refactor:`). Do not add AI-attribution lines (`Co-Authored-By`) to commits.
- **Include a test plan** in the PR description — list what you tested and how.
- **Keep PRs focused** — one logical change per PR. Large refactors should be discussed in an issue first.
- **Two-artifact versioning**: if your change affects the published API or the native layer, both `quickstore` and `quickstore-native` versions must be bumped together.

---

## Code style

### Kotlin

- Follow the [Kotlin coding conventions](https://kotlinlang.org/docs/coding-conventions.html).
- Add KDoc to every public symbol.
- KDoc belongs on `expect` declarations only (in `commonMain`) — do not duplicate it on `actual` implementations. See `ADR-1` in `especificacion-quickstore.md` for rationale.
- Getter KDoc must document the null-on-absent-key contract.
- Setter KDoc must document the `Boolean` return value as write-success.

### C++

- C++17 standard (`-std=c++17`).
- No compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- Match the style of the existing `core/` sources.

---

## Contributor licensing

By contributing to this project you agree that your contributions are licensed under the Apache License, Version 2.0. See [`LICENSE`](./LICENSE) for the full license text.
