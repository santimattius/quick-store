#pragma once
#include <cstddef>
#include <sys/file.h>
#include <unistd.h>

namespace quickstore {

enum class LockType { SharedLock, ExclusiveLock };

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

    [[nodiscard]] bool lock(LockType type) noexcept;
    [[nodiscard]] bool tryLock(LockType type) noexcept;
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
