package com.quickstore

import platform.Foundation.NSTemporaryDirectory

actual fun createTestStore(): QuickStore {
    val store = QuickStore("qs_test", NSTemporaryDirectory())
    store.clear()
    return store
}

