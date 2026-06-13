package com.quickstore

/**
 * Marks a QuickStore API as experimental.
 *
 * Stability policy: the SHAPE of an experimental API (its signature, name, or
 * existence) may change in any minor release before 1.0 WITHOUT a deprecation
 * cycle. The underlying storage BEHAVIOR is stable and MMKV-compatible — only
 * the Kotlin surface is provisional.
 *
 * Opting in via `@OptIn(ExperimentalQuickStoreApi::class)` (or propagating this
 * annotation) acknowledges that contract. Stable APIs carry no such marker and
 * follow normal semantic-versioning guarantees.
 */
@RequiresOptIn(
    message = "This API is experimental and may change in future releases.",
    level = RequiresOptIn.Level.WARNING
)
@Retention(AnnotationRetention.BINARY)
annotation class ExperimentalQuickStoreApi
