#include <jni.h>
#include "quickstore/kv_store.h"
#include <cstring>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Helper: map kv_status to a Java exception.
// Returns true if an exception was thrown (caller should return immediately).
// KV_NOT_FOUND is NOT an error — callers handle it via out-values.
// ---------------------------------------------------------------------------
static bool throwIfError(JNIEnv* env, kv_status st) {
    switch (st) {
        case KV_OK:
        case KV_NOT_FOUND:
            return false;
        case KV_IO:
            env->ThrowNew(env->FindClass("java/io/IOException"),
                          "QuickStore I/O error");
            return true;
        case KV_INVALID_ARG:
            env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"),
                          "QuickStore invalid argument");
            return true;
        case KV_BUFFER_TOO_SMALL:
            env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),
                          "QuickStore buffer too small");
            return true;
        case KV_CLOSED:
            env->ThrowNew(env->FindClass("java/lang/IllegalStateException"),
                          "QuickStore is closed");
            return true;
        default:
            env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                          "QuickStore unknown error");
            return true;
    }
}

extern "C" {

// ---------------------------------------------------------------------------
// nativeOpen(mmkvId: String, rootDir: String): Long
// Returns the handle as a Long, or 0L on failure (exception already thrown).
// ---------------------------------------------------------------------------
JNIEXPORT jlong JNICALL
Java_com_quickstore_QuickStore_nativeOpen(JNIEnv* env, jobject /*thiz*/,
                                          jstring mmkvId, jstring rootDir) {
    const char* id  = env->GetStringUTFChars(mmkvId,  nullptr);
    const char* dir = env->GetStringUTFChars(rootDir, nullptr);

    kv_store* store = nullptr;
    kv_status st = kv_open(id, dir, nullptr, &store);

    env->ReleaseStringUTFChars(mmkvId,  id);
    env->ReleaseStringUTFChars(rootDir, dir);

    if (st != KV_OK) {
        throwIfError(env, st);
        return 0L;
    }
    return reinterpret_cast<jlong>(store);
}

// ---------------------------------------------------------------------------
// nativeClose(handle: Long)
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeClose(JNIEnv* env, jobject /*thiz*/,
                                           jlong handle) {
    if (handle == 0L) return;
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    kv_status st = kv_close(store);
    throwIfError(env, st);
}

// ---------------------------------------------------------------------------
// nativeSetBool(handle: Long, key: String, value: Boolean): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeSetBool(JNIEnv* env, jobject /*thiz*/,
                                             jlong handle, jstring key,
                                             jboolean value) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    kv_status st    = kv_set_bool(store, k, static_cast<int>(value));
    env->ReleaseStringUTFChars(key, k);
    if (st != KV_OK) {
        throwIfError(env, st);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeSetLong(handle: Long, key: String, value: Long): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeSetLong(JNIEnv* env, jobject /*thiz*/,
                                             jlong handle, jstring key,
                                             jlong value) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    kv_status st    = kv_set_i64(store, k, static_cast<int64_t>(value));
    env->ReleaseStringUTFChars(key, k);
    if (st != KV_OK) {
        throwIfError(env, st);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeSetDouble(handle: Long, key: String, value: Double): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeSetDouble(JNIEnv* env, jobject /*thiz*/,
                                               jlong handle, jstring key,
                                               jdouble value) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    kv_status st    = kv_set_double(store, k, static_cast<double>(value));
    env->ReleaseStringUTFChars(key, k);
    if (st != KV_OK) {
        throwIfError(env, st);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeSetBytes(handle: Long, key: String, value: ByteArray): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeSetBytes(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle, jstring key,
                                              jbyteArray value) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);

    jsize len       = env->GetArrayLength(value);
    jbyte* data     = env->GetByteArrayElements(value, nullptr);
    kv_status st    = kv_set_bytes(store, k,
                                   reinterpret_cast<const uint8_t*>(data),
                                   static_cast<size_t>(len));
    env->ReleaseByteArrayElements(value, data, JNI_ABORT);
    env->ReleaseStringUTFChars(key, k);

    if (st != KV_OK) {
        throwIfError(env, st);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeGetBool(handle: Long, key: String, out: BooleanArray): Boolean
// Returns true if found (value written to out[0]), false if absent.
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeGetBool(JNIEnv* env, jobject /*thiz*/,
                                             jlong handle, jstring key,
                                             jbooleanArray out) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    int raw         = 0;
    kv_status st    = kv_get_bool(store, k, &raw);
    env->ReleaseStringUTFChars(key, k);

    if (st == KV_NOT_FOUND) return JNI_FALSE;
    if (throwIfError(env, st)) return JNI_FALSE;

    jboolean v = (raw != 0) ? JNI_TRUE : JNI_FALSE;
    env->SetBooleanArrayRegion(out, 0, 1, &v);
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeGetLong(handle: Long, key: String, out: LongArray): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeGetLong(JNIEnv* env, jobject /*thiz*/,
                                             jlong handle, jstring key,
                                             jlongArray out) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    int64_t raw     = 0;
    kv_status st    = kv_get_i64(store, k, &raw);
    env->ReleaseStringUTFChars(key, k);

    if (st == KV_NOT_FOUND) return JNI_FALSE;
    if (throwIfError(env, st)) return JNI_FALSE;

    jlong v = static_cast<jlong>(raw);
    env->SetLongArrayRegion(out, 0, 1, &v);
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeGetDouble(handle: Long, key: String, out: DoubleArray): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeGetDouble(JNIEnv* env, jobject /*thiz*/,
                                               jlong handle, jstring key,
                                               jdoubleArray out) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    double raw      = 0.0;
    kv_status st    = kv_get_double(store, k, &raw);
    env->ReleaseStringUTFChars(key, k);

    if (st == KV_NOT_FOUND) return JNI_FALSE;
    if (throwIfError(env, st)) return JNI_FALSE;

    jdouble v = static_cast<jdouble>(raw);
    env->SetDoubleArrayRegion(out, 0, 1, &v);
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeGetBytes(handle: Long, key: String): ByteArray?
// Returns null if not found.
// ---------------------------------------------------------------------------
JNIEXPORT jbyteArray JNICALL
Java_com_quickstore_QuickStore_nativeGetBytes(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle, jstring key) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    kv_buffer buf   = {nullptr, 0};
    kv_status st    = kv_get_bytes(store, k, &buf);
    env->ReleaseStringUTFChars(key, k);

    if (st == KV_NOT_FOUND) return nullptr;
    if (throwIfError(env, st)) return nullptr;

    jbyteArray arr = env->NewByteArray(static_cast<jsize>(buf.len));
    if (arr != nullptr && buf.len > 0) {
        env->SetByteArrayRegion(arr, 0, static_cast<jsize>(buf.len),
                                reinterpret_cast<const jbyte*>(buf.data));
    }
    kv_buffer_free(&buf);
    return arr;
}

// ---------------------------------------------------------------------------
// nativeContains(handle: Long, key: String): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeContains(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle, jstring key) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    int found       = 0;
    kv_status st    = kv_contains(store, k, &found);
    env->ReleaseStringUTFChars(key, k);
    if (throwIfError(env, st)) return JNI_FALSE;
    return (found != 0) ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------
// nativeRemove(handle: Long, key: String): Boolean
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_quickstore_QuickStore_nativeRemove(JNIEnv* env, jobject /*thiz*/,
                                            jlong handle, jstring key) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const char* k   = env->GetStringUTFChars(key, nullptr);
    kv_status st    = kv_remove(store, k);
    env->ReleaseStringUTFChars(key, k);
    if (st == KV_NOT_FOUND) return JNI_FALSE;
    if (throwIfError(env, st)) return JNI_FALSE;
    return JNI_TRUE;
}

// ---------------------------------------------------------------------------
// nativeCount(handle: Long): Long
// ---------------------------------------------------------------------------
JNIEXPORT jlong JNICALL
Java_com_quickstore_QuickStore_nativeCount(JNIEnv* env, jobject /*thiz*/,
                                           jlong handle) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    size_t count    = 0;
    kv_status st    = kv_count(store, &count);
    if (throwIfError(env, st)) return 0L;
    return static_cast<jlong>(count);
}

// ---------------------------------------------------------------------------
// nativeAllKeys(handle: Long): Array<String>
// Returns empty array for empty store.
// ---------------------------------------------------------------------------
JNIEXPORT jobjectArray JNICALL
Java_com_quickstore_QuickStore_nativeAllKeys(JNIEnv* env, jobject /*thiz*/,
                                             jlong handle) {
    kv_store* store  = reinterpret_cast<kv_store*>(handle);
    kv_buffer buf    = {nullptr, 0};
    size_t count     = 0;
    kv_status st     = kv_all_keys(store, &buf, &count);
    if (throwIfError(env, st)) return nullptr;

    jclass strClass = env->FindClass("java/lang/String");
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(count),
                                           strClass, nullptr);
    if (arr == nullptr) {
        kv_buffer_free(&buf);
        return nullptr;
    }

    if (count > 0 && buf.data != nullptr) {
        const char* p = reinterpret_cast<const char*>(buf.data);
        for (size_t i = 0; i < count; ++i) {
            jstring s = env->NewStringUTF(p);
            env->SetObjectArrayElement(arr, static_cast<jsize>(i), s);
            env->DeleteLocalRef(s);
            p += std::strlen(p) + 1;
        }
    }

    kv_buffer_free(&buf);
    return arr;
}

// ---------------------------------------------------------------------------
// nativeTrim(handle: Long)
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeTrim(JNIEnv* env, jobject /*thiz*/,
                                          jlong handle) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    kv_status st    = kv_trim(store);
    throwIfError(env, st);
}

// ---------------------------------------------------------------------------
// nativeClear(handle: Long)
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeClear(JNIEnv* env, jobject /*thiz*/,
                                           jlong handle) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    kv_status st    = kv_clear(store);
    throwIfError(env, st);
}

// ---------------------------------------------------------------------------
// nativeGetLongs(handle: Long, keys: String[], outValues: long[], outFound: boolean[])
// One JNI frame; loops kv_get_i64 over all keys. outValues[i]/outFound[i] are
// written per key. KV_NOT_FOUND -> outFound[i]=false (not an error).
// DeleteLocalRef per key is MANDATORY to avoid local-ref table overflow on
// large batches (JNI local-ref capacity default is ~512).
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeGetLongs(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle, jobjectArray keys,
                                              jlongArray outValues,
                                              jbooleanArray outFound) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const jsize n   = env->GetArrayLength(keys);

    for (jsize i = 0; i < n; ++i) {
        jstring key = static_cast<jstring>(env->GetObjectArrayElement(keys, i));
        if (key == nullptr) {
            jboolean f = JNI_FALSE;
            env->SetBooleanArrayRegion(outFound, i, 1, &f);
            continue;
        }

        const char* k = env->GetStringUTFChars(key, nullptr);
        int64_t raw   = 0;
        kv_status st  = kv_get_i64(store, k, &raw);
        env->ReleaseStringUTFChars(key, k);
        env->DeleteLocalRef(key); // MANDATORY — prevents local-ref table overflow on big batches

        jboolean found;
        if (st == KV_NOT_FOUND) {
            found = JNI_FALSE;
        } else if (st == KV_OK) {
            jlong v = static_cast<jlong>(raw);
            env->SetLongArrayRegion(outValues, i, 1, &v);
            found = JNI_TRUE;
        } else {
            // Real error: throw and abort the whole batch.
            throwIfError(env, st);
            return;
        }
        env->SetBooleanArrayRegion(outFound, i, 1, &found);
    }
}

// ---------------------------------------------------------------------------
// nativeGetBools(handle: Long, keys: String[], outValues: boolean[], outFound: boolean[])
// One JNI frame; loops kv_get_bool. kv_get_bool writes int* (0/1) — convert to
// jboolean per key. DeleteLocalRef per key MANDATORY (local-ref overflow guard).
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeGetBools(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle, jobjectArray keys,
                                              jbooleanArray outValues,
                                              jbooleanArray outFound) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const jsize n   = env->GetArrayLength(keys);

    for (jsize i = 0; i < n; ++i) {
        jstring key = static_cast<jstring>(env->GetObjectArrayElement(keys, i));
        if (key == nullptr) {
            jboolean f = JNI_FALSE;
            env->SetBooleanArrayRegion(outFound, i, 1, &f);
            continue;
        }

        const char* k = env->GetStringUTFChars(key, nullptr);
        int raw       = 0;
        kv_status st  = kv_get_bool(store, k, &raw);
        env->ReleaseStringUTFChars(key, k);
        env->DeleteLocalRef(key); // MANDATORY — prevents local-ref table overflow on big batches

        jboolean found;
        if (st == KV_NOT_FOUND) {
            found = JNI_FALSE;
        } else if (st == KV_OK) {
            jboolean v = (raw != 0) ? JNI_TRUE : JNI_FALSE;
            env->SetBooleanArrayRegion(outValues, i, 1, &v);
            found = JNI_TRUE;
        } else {
            throwIfError(env, st);
            return;
        }
        env->SetBooleanArrayRegion(outFound, i, 1, &found);
    }
}

// ---------------------------------------------------------------------------
// nativeGetDoubles(handle: Long, keys: String[], outValues: double[], outFound: boolean[])
// One JNI frame; loops kv_get_double. DeleteLocalRef per key MANDATORY.
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeGetDoubles(JNIEnv* env, jobject /*thiz*/,
                                                jlong handle, jobjectArray keys,
                                                jdoubleArray outValues,
                                                jbooleanArray outFound) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const jsize n   = env->GetArrayLength(keys);

    for (jsize i = 0; i < n; ++i) {
        jstring key = static_cast<jstring>(env->GetObjectArrayElement(keys, i));
        if (key == nullptr) {
            jboolean f = JNI_FALSE;
            env->SetBooleanArrayRegion(outFound, i, 1, &f);
            continue;
        }

        const char* k = env->GetStringUTFChars(key, nullptr);
        double raw    = 0.0;
        kv_status st  = kv_get_double(store, k, &raw);
        env->ReleaseStringUTFChars(key, k);
        env->DeleteLocalRef(key); // MANDATORY — prevents local-ref table overflow on big batches

        jboolean found;
        if (st == KV_NOT_FOUND) {
            found = JNI_FALSE;
        } else if (st == KV_OK) {
            jdouble v = static_cast<jdouble>(raw);
            env->SetDoubleArrayRegion(outValues, i, 1, &v);
            found = JNI_TRUE;
        } else {
            throwIfError(env, st);
            return;
        }
        env->SetBooleanArrayRegion(outFound, i, 1, &found);
    }
}

// ---------------------------------------------------------------------------
// nativeSetLongs(handle: Long, keys: String[], values: long[])
// Writes each key→value pair via kv_set_i64. DeleteLocalRef per key MANDATORY
// to prevent local-ref table overflow on large batches (chunk size 500).
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeSetLongs(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle, jobjectArray keys,
                                              jlongArray values) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const jsize n   = env->GetArrayLength(keys);
    jlong* vals     = env->GetLongArrayElements(values, nullptr);
    for (jsize i = 0; i < n; ++i) {
        jstring key = static_cast<jstring>(env->GetObjectArrayElement(keys, i));
        if (key == nullptr) continue;
        const char* k = env->GetStringUTFChars(key, nullptr);
        kv_set_i64(store, k, static_cast<int64_t>(vals[i]));
        env->ReleaseStringUTFChars(key, k);
        env->DeleteLocalRef(key); // MANDATORY — prevents local-ref table overflow on big batches
    }
    env->ReleaseLongArrayElements(values, vals, JNI_ABORT);
}

// ---------------------------------------------------------------------------
// nativeSetBools(handle: Long, keys: String[], values: boolean[])
// Writes each key→value pair via kv_set_bool (0/1 int). DeleteLocalRef MANDATORY.
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeSetBools(JNIEnv* env, jobject /*thiz*/,
                                              jlong handle, jobjectArray keys,
                                              jbooleanArray values) {
    kv_store* store  = reinterpret_cast<kv_store*>(handle);
    const jsize n    = env->GetArrayLength(keys);
    jboolean* vals   = env->GetBooleanArrayElements(values, nullptr);
    for (jsize i = 0; i < n; ++i) {
        jstring key = static_cast<jstring>(env->GetObjectArrayElement(keys, i));
        if (key == nullptr) continue;
        const char* k = env->GetStringUTFChars(key, nullptr);
        kv_set_bool(store, k, vals[i] ? 1 : 0);
        env->ReleaseStringUTFChars(key, k);
        env->DeleteLocalRef(key); // MANDATORY — prevents local-ref table overflow on big batches
    }
    env->ReleaseBooleanArrayElements(values, vals, JNI_ABORT);
}

// ---------------------------------------------------------------------------
// nativeSetDoubles(handle: Long, keys: String[], values: double[])
// Writes each key→value pair via kv_set_double. DeleteLocalRef MANDATORY.
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_quickstore_QuickStore_nativeSetDoubles(JNIEnv* env, jobject /*thiz*/,
                                                jlong handle, jobjectArray keys,
                                                jdoubleArray values) {
    kv_store* store = reinterpret_cast<kv_store*>(handle);
    const jsize n   = env->GetArrayLength(keys);
    jdouble* vals   = env->GetDoubleArrayElements(values, nullptr);
    for (jsize i = 0; i < n; ++i) {
        jstring key = static_cast<jstring>(env->GetObjectArrayElement(keys, i));
        if (key == nullptr) continue;
        const char* k = env->GetStringUTFChars(key, nullptr);
        kv_set_double(store, k, static_cast<double>(vals[i]));
        env->ReleaseStringUTFChars(key, k);
        env->DeleteLocalRef(key); // MANDATORY — prevents local-ref table overflow on big batches
    }
    env->ReleaseDoubleArrayElements(values, vals, JNI_ABORT);
}

} // extern "C"
