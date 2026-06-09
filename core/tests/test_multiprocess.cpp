#if defined(__linux__) || defined(__APPLE__)
#include "quickstore/kv_store.h"
#include <gtest/gtest.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <string>
#include <cerrno>

// ---------------------------------------------------------------------------
// TempDir RAII helper — creates an isolated directory for each test
// ---------------------------------------------------------------------------
struct TempDir {
    char path[64];
    TempDir() {
        std::strncpy(path, "/tmp/qs_mp_test_XXXXXX", sizeof(path));
        EXPECT_NE(::mkdtemp(path), nullptr) << "mkdtemp failed: " << ::strerror(errno);
    }
    ~TempDir() {
        // Best-effort cleanup: remove all files then the directory
        std::string cmd = "rm -rf '";
        cmd += path;
        cmd += "'";
        ::system(cmd.c_str());
    }
    const char* c_str() const { return path; }
};

// ---------------------------------------------------------------------------
// TC-MP-1: Parent writes 50 keys (multi_process=1), child opens same file and
// reads all 50 keys — all must be present.
// ---------------------------------------------------------------------------
TEST(TC_MP_1, ParentWriteChildRead) {
    TempDir dir;

    kv_options opts{};
    opts.multi_process = 1;

    // Parent writes 50 keys
    {
        kv_store* s = nullptr;
        ASSERT_EQ(kv_open("tc_mp1", dir.c_str(), &opts, &s), KV_OK);
        ASSERT_NE(s, nullptr);

        for (int i = 0; i < 50; ++i) {
            std::string key = "key_" + std::to_string(i);
            int64_t val = static_cast<int64_t>(i * 100);
            ASSERT_EQ(kv_set_i64(s, key.c_str(), val), KV_OK) << "Failed at key " << key;
        }

        kv_close(s);
    }

    // Fork child to verify all 50 keys are readable
    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed";

    if (pid == 0) {
        // CHILD
        kv_store* s = nullptr;
        if (kv_open("tc_mp1", dir.c_str(), &opts, &s) != KV_OK) ::_exit(1);

        for (int i = 0; i < 50; ++i) {
            std::string key = "key_" + std::to_string(i);
            int64_t val = 0;
            if (kv_get_i64(s, key.c_str(), &val) != KV_OK) {
                kv_close(s);
                ::_exit(2);
            }
            if (val != static_cast<int64_t>(i * 100)) {
                kv_close(s);
                ::_exit(3);
            }
        }

        kv_close(s);
        ::_exit(0);
    } else {
        // PARENT
        int status = 0;
        ASSERT_NE(::waitpid(pid, &status, 0), -1);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0) << "Child failed to read all keys";
    }
}

// ---------------------------------------------------------------------------
// TC-MP-2: Parent and child each write 50 unique keys concurrently (both
// multi_process=1). After both finish, total must be 100 keys with no
// corruption. This is the C1 regression guard.
// ---------------------------------------------------------------------------
TEST(TC_MP_2, ConcurrentWritesNoCorruption) {
    TempDir dir;

    kv_options opts{};
    opts.multi_process = 1;

    // Pre-create the store so both processes open an existing file
    {
        kv_store* s = nullptr;
        ASSERT_EQ(kv_open("tc_mp2", dir.c_str(), &opts, &s), KV_OK);
        ASSERT_NE(s, nullptr);
        kv_close(s);
    }

    // Synchronization pipe: parent waits until child is done
    int sync_pipe[2];
    ASSERT_EQ(::pipe(sync_pipe), 0);

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed";

    if (pid == 0) {
        // CHILD: write keys child_0..child_49
        ::close(sync_pipe[0]);
        kv_store* s = nullptr;
        if (kv_open("tc_mp2", dir.c_str(), &opts, &s) != KV_OK) ::_exit(1);

        for (int i = 0; i < 50; ++i) {
            std::string key = "child_" + std::to_string(i);
            int64_t val = static_cast<int64_t>(i + 1000);
            if (kv_set_i64(s, key.c_str(), val) != KV_OK) {
                kv_close(s);
                ::_exit(2);
            }
        }

        kv_close(s);
        char done = 'D';
        ::write(sync_pipe[1], &done, 1);
        ::close(sync_pipe[1]);
        ::_exit(0);
    } else {
        // PARENT: write keys parent_0..parent_49 concurrently with child
        ::close(sync_pipe[1]);

        kv_store* s = nullptr;
        ASSERT_EQ(kv_open("tc_mp2", dir.c_str(), &opts, &s), KV_OK);
        ASSERT_NE(s, nullptr);

        for (int i = 0; i < 50; ++i) {
            std::string key = "parent_" + std::to_string(i);
            int64_t val = static_cast<int64_t>(i + 2000);
            ASSERT_EQ(kv_set_i64(s, key.c_str(), val), KV_OK);
        }

        kv_close(s);

        // Wait for child to finish
        char done;
        ASSERT_EQ(::read(sync_pipe[0], &done, 1), 1);
        ::close(sync_pipe[0]);

        int status = 0;
        ASSERT_NE(::waitpid(pid, &status, 0), -1);
        ASSERT_TRUE(WIFEXITED(status));
        ASSERT_EQ(WEXITSTATUS(status), 0) << "Child failed to write its keys";

        // Reopen and verify all 100 keys exist
        kv_store* verify = nullptr;
        ASSERT_EQ(kv_open("tc_mp2", dir.c_str(), &opts, &verify), KV_OK);
        ASSERT_NE(verify, nullptr);

        size_t total = 0;
        ASSERT_EQ(kv_count(verify, &total), KV_OK);
        EXPECT_EQ(total, 100u) << "Expected 100 keys, got " << total;

        // Spot-check some keys from both processes
        for (int i = 0; i < 10; ++i) {
            int64_t val = 0;
            std::string pk = "parent_" + std::to_string(i);
            EXPECT_EQ(kv_get_i64(verify, pk.c_str(), &val), KV_OK) << "Missing " << pk;
            if (val != 0) EXPECT_EQ(val, i + 2000);

            std::string ck = "child_" + std::to_string(i);
            EXPECT_EQ(kv_get_i64(verify, ck.c_str(), &val), KV_OK) << "Missing " << ck;
            if (val != 0) EXPECT_EQ(val, i + 1000);
        }

        kv_close(verify);
    }
}

// ---------------------------------------------------------------------------
// TC-MP-3: Single-process mode (multi_process=0) still works — basic round-trip
// ---------------------------------------------------------------------------
TEST(TC_MP_3, SingleProcessModeRoundTrip) {
    TempDir dir;

    // Open with multi_process=0 (default/explicit)
    kv_store* s = nullptr;
    ASSERT_EQ(kv_open("tc_mp3", dir.c_str(), nullptr, &s), KV_OK);
    ASSERT_NE(s, nullptr);

    ASSERT_EQ(kv_set_i64(s, "hello", 42), KV_OK);

    int64_t val = 0;
    ASSERT_EQ(kv_get_i64(s, "hello", &val), KV_OK);
    EXPECT_EQ(val, 42);

    kv_close(s);

    // Reopen and verify persistence
    kv_store* s2 = nullptr;
    ASSERT_EQ(kv_open("tc_mp3", dir.c_str(), nullptr, &s2), KV_OK);
    ASSERT_NE(s2, nullptr);

    int64_t val2 = 0;
    ASSERT_EQ(kv_get_i64(s2, "hello", &val2), KV_OK);
    EXPECT_EQ(val2, 42);

    kv_close(s2);
}

// ---------------------------------------------------------------------------
// TC-MP-4: kv_open with multi_process=1 on a new directory creates the store
// correctly and round-trips data.
// ---------------------------------------------------------------------------
TEST(TC_MP_4, MultiProcessOpenAndRoundTrip) {
    TempDir dir;

    kv_options opts{};
    opts.multi_process = 1;

    kv_store* s = nullptr;
    ASSERT_EQ(kv_open("tc_mp4", dir.c_str(), &opts, &s), KV_OK);
    ASSERT_NE(s, nullptr);

    ASSERT_EQ(kv_set_i64(s, "mp_key", 12345), KV_OK);

    int64_t val = 0;
    ASSERT_EQ(kv_get_i64(s, "mp_key", &val), KV_OK);
    EXPECT_EQ(val, 12345);

    kv_close(s);

    // Reopen multi-process and verify
    kv_store* s2 = nullptr;
    ASSERT_EQ(kv_open("tc_mp4", dir.c_str(), &opts, &s2), KV_OK);
    ASSERT_NE(s2, nullptr);

    int64_t val2 = 0;
    ASSERT_EQ(kv_get_i64(s2, "mp_key", &val2), KV_OK);
    EXPECT_EQ(val2, 12345);

    kv_close(s2);
}

// ---------------------------------------------------------------------------
// TC-MP-5: Two processes, one calls kv_trim (compact), other reads after.
// File is valid after compact; second process can read all keys.
// ---------------------------------------------------------------------------
TEST(TC_MP_5, TrimUnderMultiProcess) {
    TempDir dir;

    kv_options opts{};
    opts.multi_process = 1;

    // Write initial keys in parent
    {
        kv_store* s = nullptr;
        ASSERT_EQ(kv_open("tc_mp5", dir.c_str(), &opts, &s), KV_OK);
        ASSERT_NE(s, nullptr);

        for (int i = 0; i < 20; ++i) {
            std::string key = "trim_key_" + std::to_string(i);
            ASSERT_EQ(kv_set_i64(s, key.c_str(), static_cast<int64_t>(i)), KV_OK);
        }

        // Overwrite some keys to create fragmentation
        for (int i = 0; i < 10; ++i) {
            std::string key = "trim_key_" + std::to_string(i);
            ASSERT_EQ(kv_set_i64(s, key.c_str(), static_cast<int64_t>(i + 500)), KV_OK);
        }

        // Compact
        ASSERT_EQ(kv_trim(s), KV_OK);
        kv_close(s);
    }

    // Fork child to verify all keys after trim
    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed";

    if (pid == 0) {
        // CHILD
        kv_store* s = nullptr;
        if (kv_open("tc_mp5", dir.c_str(), &opts, &s) != KV_OK) ::_exit(1);

        // Check keys 0..9 have the updated value (i+500)
        for (int i = 0; i < 10; ++i) {
            std::string key = "trim_key_" + std::to_string(i);
            int64_t val = 0;
            if (kv_get_i64(s, key.c_str(), &val) != KV_OK) {
                kv_close(s);
                ::_exit(2);
            }
            if (val != static_cast<int64_t>(i + 500)) {
                kv_close(s);
                ::_exit(3);
            }
        }

        // Check keys 10..19 have original values
        for (int i = 10; i < 20; ++i) {
            std::string key = "trim_key_" + std::to_string(i);
            int64_t val = 0;
            if (kv_get_i64(s, key.c_str(), &val) != KV_OK) {
                kv_close(s);
                ::_exit(4);
            }
            if (val != static_cast<int64_t>(i)) {
                kv_close(s);
                ::_exit(5);
            }
        }

        kv_close(s);
        ::_exit(0);
    } else {
        // PARENT
        int status = 0;
        ASSERT_NE(::waitpid(pid, &status, 0), -1);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0)
            << "Child failed after trim. Exit code: " << WEXITSTATUS(status);
    }
}

#endif // __linux__ || __APPLE__
