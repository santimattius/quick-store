#include "quickstore/file_lock.h"
#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>

using quickstore::FileLock;
using quickstore::LockType;

// Helper: create a temporary file and return its fd.
// Caller is responsible for ::close() and unlink.
static int makeTmpFd(char* tmpl) {
    int fd = ::mkstemp(tmpl);
    EXPECT_GE(fd, 0) << "mkstemp failed: " << ::strerror(errno);
    return fd;
}

// TC-FLOCK-1: FileLock(-1).lock(ExclusiveLock) returns false (invalid fd no-op)
TEST(TC_FLOCK_1, InvalidFdExclusive) {
    FileLock lk(-1);
    EXPECT_FALSE(lk.lock(LockType::ExclusiveLock));
}

// TC-FLOCK-1b: FileLock(-1).lock(SharedLock) returns false
TEST(TC_FLOCK_1b, InvalidFdShared) {
    FileLock lk(-1);
    EXPECT_FALSE(lk.lock(LockType::SharedLock));
}

// TC-FLOCK-2: lock(ExclusiveLock) returns true on valid tmpfile fd
TEST(TC_FLOCK_2, ExclusiveLockValidFd) {
    char tmpl[] = "/tmp/qs_flock_test_XXXXXX";
    int fd = makeTmpFd(tmpl);
    ASSERT_GE(fd, 0);

    {
        FileLock lk(fd);
        EXPECT_TRUE(lk.lock(LockType::ExclusiveLock));
        lk.unlock(LockType::ExclusiveLock);
    }

    ::close(fd);
    ::unlink(tmpl);
}

// TC-FLOCK-3: lock(ExclusiveLock) twice (same process) returns true (reentrant)
TEST(TC_FLOCK_3, ExclusiveLockReentrant) {
    char tmpl[] = "/tmp/qs_flock_reentrant_XXXXXX";
    int fd = makeTmpFd(tmpl);
    ASSERT_GE(fd, 0);

    {
        FileLock lk(fd);
        EXPECT_TRUE(lk.lock(LockType::ExclusiveLock));
        EXPECT_TRUE(lk.lock(LockType::ExclusiveLock));  // reentrant — must not deadlock
        lk.unlock(LockType::ExclusiveLock);
        lk.unlock(LockType::ExclusiveLock);
    }

    ::close(fd);
    ::unlink(tmpl);
}

// TC-FLOCK-4: lock(SharedLock) returns true
TEST(TC_FLOCK_4, SharedLockValidFd) {
    char tmpl[] = "/tmp/qs_flock_shared_XXXXXX";
    int fd = makeTmpFd(tmpl);
    ASSERT_GE(fd, 0);

    {
        FileLock lk(fd);
        EXPECT_TRUE(lk.lock(LockType::SharedLock));
        lk.unlock(LockType::SharedLock);
    }

    ::close(fd);
    ::unlink(tmpl);
}

// TC-FLOCK-5: lock(SharedLock) twice (same process) returns true (reentrant)
TEST(TC_FLOCK_5, SharedLockReentrant) {
    char tmpl[] = "/tmp/qs_flock_shared_reentrant_XXXXXX";
    int fd = makeTmpFd(tmpl);
    ASSERT_GE(fd, 0);

    {
        FileLock lk(fd);
        EXPECT_TRUE(lk.lock(LockType::SharedLock));
        EXPECT_TRUE(lk.lock(LockType::SharedLock));  // reentrant
        lk.unlock(LockType::SharedLock);
        lk.unlock(LockType::SharedLock);
    }

    ::close(fd);
    ::unlink(tmpl);
}

// TC-FLOCK-6: unlock() after lock — subsequent tryLock from same fd returns true again
TEST(TC_FLOCK_6, UnlockAndRetryTryLock) {
    char tmpl[] = "/tmp/qs_flock_unlock_retry_XXXXXX";
    int fd = makeTmpFd(tmpl);
    ASSERT_GE(fd, 0);

    {
        FileLock lk(fd);
        EXPECT_TRUE(lk.lock(LockType::ExclusiveLock));
        lk.unlock(LockType::ExclusiveLock);
        // After unlock, tryLock should succeed again
        EXPECT_TRUE(lk.tryLock(LockType::ExclusiveLock));
        lk.unlock(LockType::ExclusiveLock);
    }

    ::close(fd);
    ::unlink(tmpl);
}

// TC-FLOCK-7: fork() — parent holds ExclusiveLock, child calls tryLock(ExclusiveLock)
// on same fd → must return false; parent unlocks; child retries → true.
// Uses a pipe to synchronize: parent signals child when lock is held, and child signals
// parent when it has confirmed the blocked state.
TEST(TC_FLOCK_7, CrossProcessExclusiveLock) {
    char tmpl[] = "/tmp/qs_flock_fork_XXXXXX";
    int fd = makeTmpFd(tmpl);
    ASSERT_GE(fd, 0);

    // pipe[0] = read end, pipe[1] = write end
    int pipeParentToChild[2];  // parent signals child: "lock is held"
    int pipeChildToParent[2];  // child signals parent: "tryLock done"
    ASSERT_EQ(::pipe(pipeParentToChild), 0);
    ASSERT_EQ(::pipe(pipeChildToParent), 0);

    pid_t pid = ::fork();
    ASSERT_NE(pid, -1) << "fork failed";

    if (pid == 0) {
        // ---- CHILD ----
        ::close(pipeParentToChild[1]);
        ::close(pipeChildToParent[0]);

        // Wait for parent to signal "lock held"
        char buf;
        if (::read(pipeParentToChild[0], &buf, 1) != 1) ::_exit(2);

        // Open a NEW independent fd — flock() requires separate open-file-descriptions
        // for cross-process mutual exclusion to work correctly on macOS and Linux.
        int childFd = ::open(tmpl, O_RDWR);
        if (childFd < 0) ::_exit(5);

        FileLock childLk(childFd);
        bool firstTry = childLk.tryLock(LockType::ExclusiveLock);

        // Signal parent: "I've tried, result = firstTry"
        char result = firstTry ? 1 : 0;
        if (::write(pipeChildToParent[1], &result, 1) != 1) { ::close(childFd); ::_exit(3); }

        // Wait for parent to signal "lock released"
        if (::read(pipeParentToChild[0], &buf, 1) != 1) { ::close(childFd); ::_exit(4); }

        // Now the lock should be available
        bool secondTry = childLk.tryLock(LockType::ExclusiveLock);
        if (secondTry) {
            childLk.unlock(LockType::ExclusiveLock);
        }

        ::close(childFd);

        // Exit code: 0 = success (first=false, second=true), else failure
        ::_exit((firstTry == false && secondTry == true) ? 0 : 1);
    } else {
        // ---- PARENT ----
        ::close(pipeParentToChild[0]);
        ::close(pipeChildToParent[1]);

        FileLock parentLk(fd);
        ASSERT_TRUE(parentLk.lock(LockType::ExclusiveLock));

        // Signal child: "lock is now held"
        char go = 'G';
        ASSERT_EQ(::write(pipeParentToChild[1], &go, 1), 1);

        // Wait for child to attempt tryLock
        char childResult;
        ASSERT_EQ(::read(pipeChildToParent[0], &childResult, 1), 1);

        // child should have gotten false (blocked by parent's exclusive lock)
        EXPECT_EQ(childResult, 0) << "Child should not have acquired lock while parent holds it";

        // Release the lock
        parentLk.unlock(LockType::ExclusiveLock);

        // Signal child: "lock released"
        ASSERT_EQ(::write(pipeParentToChild[1], &go, 1), 1);

        // Wait for child
        int status = 0;
        ASSERT_NE(::waitpid(pid, &status, 0), -1);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0) << "Child process reported failure";

        ::close(pipeParentToChild[1]);
        ::close(pipeChildToParent[0]);
    }

    ::close(fd);
    ::unlink(tmpl);
}
