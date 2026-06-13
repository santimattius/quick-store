#pragma once
#include <cstddef>
#include <sys/file.h>
#include <unistd.h>

namespace quickstore {

enum class LockType {
    SharedLock,    // Allows concurrent readers; maps to LOCK_SH.
    ExclusiveLock  // Exclusive write access; maps to LOCK_EX.
};

/* Reentrant wrapper around flock(2) for one open file descriptor. Tracks shared
   and exclusive lock counts so nested lock()/unlock() calls in the same process
   do not deadlock and the OS lock is taken once and released only when the last
   holder unlocks. flock(2) is per-open-file-description, so distinct fds across
   processes still enforce mutual exclusion. Non-copyable, non-movable: the lock
   state is tied to m_fd's lifetime. */
class FileLock {
public:
    explicit FileLock(int fd) noexcept : m_fd(fd) {}
    ~FileLock() noexcept {
        if (m_sharedLockCount > 0 || m_exclusiveLockCount > 0) {
            flockRelease();
        }
    }

    FileLock(const FileLock&)            = delete;
    FileLock& operator=(const FileLock&) = delete;
    FileLock(FileLock&&)                 = delete;
    FileLock& operator=(FileLock&&)      = delete;

    // Acquires the lock, BLOCKING until granted. Reentrant: nested calls bump a counter. An exclusive holder already satisfies a shared request. Promotion shared->exclusive briefly releases the OS lock (a peer may interleave in that window). Returns false only on flock failure (e.g. bad fd).
    // The underlying kernel lock (flock) is acquired only on the first call; subsequent
    // reentrant calls increment the reference count without a syscall.
    [[nodiscard]] bool lock(LockType type) noexcept;
    // Non-blocking variant of lock(). Returns false immediately if the lock is unavailable. On a failed shared->exclusive promotion it best-effort re-acquires the shared lock before returning false.
    [[nodiscard]] bool tryLock(LockType type) noexcept;
    // Decrements the matching counter; releases the OS lock only when both counts reach zero. Releasing exclusive while a shared count remains DOWNGRADES to a shared lock. No-op if the matching count is already zero.
    void unlock(LockType type) noexcept;

private:
    int    m_fd;
    size_t m_sharedLockCount    = 0;
    size_t m_exclusiveLockCount = 0;

    // flock(2) is per-open-file-description: independently opened fds in different
    // processes correctly enforce mutual exclusion, which is what we need for TC_MP_2.
    bool flockAcquire(bool exclusive, bool wait) noexcept {
        if (m_fd < 0) return false;
        int how = exclusive ? LOCK_EX : LOCK_SH;
        if (!wait) how |= LOCK_NB;
        return ::flock(m_fd, how) == 0;
    }

    void flockRelease() noexcept {
        if (m_fd < 0) return;
        ::flock(m_fd, LOCK_UN);
    }
};

} // namespace quickstore
