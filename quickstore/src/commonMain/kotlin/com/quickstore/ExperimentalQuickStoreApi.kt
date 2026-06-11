package com.quickstore

@RequiresOptIn(
    message = "This API is experimental and may change in future releases.",
    level = RequiresOptIn.Level.WARNING
)
@Retention(AnnotationRetention.BINARY)
annotation class ExperimentalQuickStoreApi
