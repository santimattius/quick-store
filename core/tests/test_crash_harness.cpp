// test_crash_harness.cpp
// Crash-consistency harness for quickstore.
//
// Uses fork() + _exit() to simulate a hard process crash (SIGKILL semantics)
// without invoking C++ destructors or calling kv_close. On macOS/APFS and Linux,
// MAP_SHARED mmap pages and pwrite'd .crc bytes survive process exit in the
// kernel page cache — this harness tests that contract, NOT power-loss recovery.
//
// TC_CRASH_3 and TC_CRASH_4 corrupt only bytes 0–27 of the .crc file (primary
// CRC digest + version + sequence + vector), leaving lastConfirmedActualSize and
// lastConfirmedCRCDigest (at offsets 32–40) intact. This simulates a realistic
// torn write to the .crc header while allowing fallback recovery via lastConfirmed.

#if defined(__linux__) || defined(__APPLE__)

#include "quickstore/kv_store.h"
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <cerrno>

// ---------------------------------------------------------------------------
// TempDir RAII — creates an isolated directory per test (same pattern as
// test_multiprocess.cpp).
// ---------------------------------------------------------------------------
struct CrashTempDir {
    char path[64];
    CrashTempDir() {
        std::strncpy(path, "/tmp/qs_crash_XXXXXX", sizeof(path));
        EXPECT_NE(::mkdtemp(path), nullptr)
            << "mkdtemp failed: " << ::strerror(errno);
    }
    ~CrashTempDir() {
        std::string cmd = "rm -rf '";
        cmd += path;
        cmd += "'";
        ::system(cmd.c_str());
    }
    const char* c_str() const { return path; }
};

// ---------------------------------------------------------------------------
// TC_CRASH_1 — process crash BEFORE compact (no kv_close called).
//
// Child writes 20 keys then calls _exit(0) — C++ destructors do NOT run, so
// the QuickStoreWriter dtor (which would call close()) is skipped. Both the
// MAP_SHARED data pages and the pwrite'd .crc bytes survive in the kernel
// page cache. Parent reopens and verifies all 20 keys are present.
// ---------------------------------------------------------------------------
TEST(CrashHarness, TC_CRASH_1_NoCompact) {
    CrashTempDir dir;
    const char* store_id = "tc_crash1";

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed: " << ::strerror(errno);

    if (pid == 0) {
        // CHILD: open, write 20 keys, crash (no kv_close, no compact)
        kv_store* s = nullptr;
        if (kv_open(store_id, dir.c_str(), nullptr, &s) != KV_OK) {
            ::_exit(1);
        }
        for (int i = 0; i < 20; ++i) {
            std::string key = "k" + std::to_string(i);
            if (kv_set_i64(s, key.c_str(), static_cast<int64_t>(i)) != KV_OK) {
                ::_exit(2);
            }
        }
        // Crash without kv_close — _exit skips all C++ destructors
        ::_exit(0);
    }

    // PARENT: wait for child to crash, then reopen
    int status = 0;
    ASSERT_NE(::waitpid(pid, &status, 0), -1);
    ASSERT_TRUE(WIFEXITED(status));
    // Child exit code 0 = wrote all keys successfully before crash
    EXPECT_EQ(WEXITSTATUS(status), 0);

    kv_store* s = nullptr;
    ASSERT_EQ(kv_open(store_id, dir.c_str(), nullptr, &s), KV_OK);
    ASSERT_NE(s, nullptr);

    // All 20 writes are in the page cache — they must be visible after reopen
    size_t count = 0;
    ASSERT_EQ(kv_count(s, &count), KV_OK);
    EXPECT_EQ(count, 20u)
        << "Expected 20 keys in page cache after process crash";

    kv_close(s);
}

// ---------------------------------------------------------------------------
// TC_CRASH_2 — compact then write more keys, then crash.
//
// kv_trim() calls doFullWriteback which advances lastConfirmedActualSize.
// Both the compacted data and the 10 post-compact appends survive in page
// cache across _exit. Parent reopens and expects all 30 keys.
// ---------------------------------------------------------------------------
TEST(CrashHarness, TC_CRASH_2_CompactThenCrash) {
    CrashTempDir dir;
    const char* store_id = "tc_crash2";

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed: " << ::strerror(errno);

    if (pid == 0) {
        // CHILD: write 20 → compact → write 10 more → crash
        kv_store* s = nullptr;
        if (kv_open(store_id, dir.c_str(), nullptr, &s) != KV_OK) {
            ::_exit(1);
        }
        for (int i = 0; i < 20; ++i) {
            std::string key = "k" + std::to_string(i);
            if (kv_set_i64(s, key.c_str(), static_cast<int64_t>(i)) != KV_OK) {
                ::_exit(2);
            }
        }
        if (kv_trim(s) != KV_OK) {
            ::_exit(3);
        }
        for (int i = 20; i < 30; ++i) {
            std::string key = "k" + std::to_string(i);
            if (kv_set_i64(s, key.c_str(), static_cast<int64_t>(i)) != KV_OK) {
                ::_exit(4);
            }
        }
        // Crash — no kv_close
        ::_exit(0);
    }

    // PARENT
    int status = 0;
    ASSERT_NE(::waitpid(pid, &status, 0), -1);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);

    kv_store* s = nullptr;
    ASSERT_EQ(kv_open(store_id, dir.c_str(), nullptr, &s), KV_OK);
    ASSERT_NE(s, nullptr);

    // Compact + post-compact appends both survive in page cache
    size_t count = 0;
    ASSERT_EQ(kv_count(s, &count), KV_OK);
    EXPECT_EQ(count, 30u)
        << "Expected 30 keys (20 compacted + 10 appended) after crash";

    kv_close(s);
}

// ---------------------------------------------------------------------------
// TC_CRASH_3 — partial .crc corruption, no prior compact.
//
// Child writes 20 keys, then deliberately corrupts bytes 0–27 of the .crc
// file (primary CRC digest, version, sequence, and vector fields). Bytes 28+
// (m_actualSize, lastConfirmedActualSize=4, lastConfirmedCRCDigest) are left
// intact. This simulates a torn write to the .crc header.
//
// On reopen: primary CRC validation fails (0xFF crcDigest does not match),
// but fallback to lastConfirmedActualSize (=4, the empty initial state) and
// lastConfirmedCRCDigest succeeds. kv_open returns KV_OK with count == 0
// (the 20 writes are sacrificed; no compact was ever called).
// ---------------------------------------------------------------------------
TEST(CrashHarness, TC_CRASH_3_CorruptCrc_NoCompact) {
    CrashTempDir dir;
    const char* store_id = "tc_crash3";

    // Build the .crc path: dir/store_id.crc
    std::string crc_path = std::string(dir.c_str()) + "/" + store_id + ".crc";

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed: " << ::strerror(errno);

    if (pid == 0) {
        // CHILD: write 20 keys, then corrupt primary .crc header, then crash
        kv_store* s = nullptr;
        if (kv_open(store_id, dir.c_str(), nullptr, &s) != KV_OK) {
            ::_exit(1);
        }
        for (int i = 0; i < 20; ++i) {
            std::string key = "k" + std::to_string(i);
            if (kv_set_i64(s, key.c_str(), static_cast<int64_t>(i)) != KV_OK) {
                ::_exit(2);
            }
        }

        // Corrupt bytes 0–27: crcDigest(4) + version(4) + sequence(4) + vector(16) = 28 bytes.
        // Bytes 28+: m_actualSize, lastConfirmedActualSize, lastConfirmedCRCDigest — left intact.
        // This is a realistic partial write to the .crc header section.
        int crcfd = ::open(crc_path.c_str(), O_RDWR);
        if (crcfd < 0) {
            ::_exit(3);
        }
        uint8_t garbage[28];
        ::memset(garbage, 0xFF, sizeof(garbage));
        if (::pwrite(crcfd, garbage, sizeof(garbage), 0) != static_cast<ssize_t>(sizeof(garbage))) {
            ::close(crcfd);
            ::_exit(4);
        }
        ::close(crcfd);

        // Crash without kv_close
        ::_exit(0);
    }

    // PARENT
    int status = 0;
    ASSERT_NE(::waitpid(pid, &status, 0), -1);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);

    kv_store* s = nullptr;
    // Must recover to KV_OK (never KV_CORRUPT) via lastConfirmed fallback
    ASSERT_EQ(kv_open(store_id, dir.c_str(), nullptr, &s), KV_OK)
        << "kv_open must recover via lastConfirmed fallback, not return KV_CORRUPT";
    ASSERT_NE(s, nullptr);

    // lastConfirmedActualSize=4 (empty state, no compact ever called)
    // so the recovered store has 0 keys
    size_t count = 0;
    ASSERT_EQ(kv_count(s, &count), KV_OK);
    EXPECT_EQ(count, 0u)
        << "Expected 0 keys: lastConfirmed points to empty state (no compact was called)";

    kv_close(s);
}

// ---------------------------------------------------------------------------
// TC_CRASH_4 — partial .crc corruption AFTER compact baseline established.
//
// Child writes 20 keys, calls kv_trim() (lastConfirmedActualSize advances to
// the compact state), writes 10 more, then corrupts bytes 0–27 of the .crc.
// Parent reopens with multi_process=1 (validates flock is released on _exit).
//
// Primary CRC fails, fallback to lastConfirmed succeeds with the compact
// baseline: KV_OK + count == 20. The 10 post-compact appends are sacrificed.
// ---------------------------------------------------------------------------
TEST(CrashHarness, TC_CRASH_4_CorruptCrc_WithCompactBaseline) {
    CrashTempDir dir;
    const char* store_id = "tc_crash4";

    std::string crc_path = std::string(dir.c_str()) + "/" + store_id + ".crc";

    kv_options mp_opts{};
    mp_opts.multi_process = 1;

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed: " << ::strerror(errno);

    if (pid == 0) {
        // CHILD: write 20 → compact (lastConfirmed=20) → write 10 more → corrupt .crc → crash
        kv_store* s = nullptr;
        if (kv_open(store_id, dir.c_str(), &mp_opts, &s) != KV_OK) {
            ::_exit(1);
        }
        for (int i = 0; i < 20; ++i) {
            std::string key = "k" + std::to_string(i);
            if (kv_set_i64(s, key.c_str(), static_cast<int64_t>(i)) != KV_OK) {
                ::_exit(2);
            }
        }
        if (kv_trim(s) != KV_OK) {
            ::_exit(3);
        }
        for (int i = 20; i < 30; ++i) {
            std::string key = "k" + std::to_string(i);
            if (kv_set_i64(s, key.c_str(), static_cast<int64_t>(i)) != KV_OK) {
                ::_exit(4);
            }
        }

        // Corrupt bytes 0–27 only, leaving lastConfirmed at offsets 32–40 intact
        int crcfd = ::open(crc_path.c_str(), O_RDWR);
        if (crcfd < 0) {
            ::_exit(5);
        }
        uint8_t garbage[28];
        ::memset(garbage, 0xFF, sizeof(garbage));
        if (::pwrite(crcfd, garbage, sizeof(garbage), 0) != static_cast<ssize_t>(sizeof(garbage))) {
            ::close(crcfd);
            ::_exit(6);
        }
        ::close(crcfd);

        // Crash — _exit releases flock on child's store fd automatically
        ::_exit(0);
    }

    // PARENT: waitpid — child should have completed and exited
    int status = 0;
    ASSERT_NE(::waitpid(pid, &status, 0), -1);
    ASSERT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);

    // Open with multi_process=1 — verifies flock is NOT held by crashed child
    // (OS releases all fds, including flock, on _exit)
    kv_store* s = nullptr;
    ASSERT_EQ(kv_open(store_id, dir.c_str(), &mp_opts, &s), KV_OK)
        << "kv_open must not block or fail — flock must be released on child _exit";
    ASSERT_NE(s, nullptr);

    // Fallback to lastConfirmed = compact state at 20 keys
    // (10 post-compact appends sacrificed due to .crc corruption)
    size_t count = 0;
    ASSERT_EQ(kv_count(s, &count), KV_OK);
    EXPECT_EQ(count, 20u)
        << "Expected 20 keys: lastConfirmed points to compact baseline";

    kv_close(s);
}

#endif // __linux__ || __APPLE__
