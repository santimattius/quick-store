#include "quickstore/file_lock.h"

namespace quickstore {

bool FileLock::lock(LockType type) noexcept {
    if (type == LockType::SharedLock) {
        // Already holding exclusive — stronger lock already satisfies shared
        if (m_exclusiveLockCount > 0) {
            m_sharedLockCount++;
            return true;
        }
        // Reentrant shared lock
        if (m_sharedLockCount > 0) {
            m_sharedLockCount++;
            return true;
        }
        if (!flockAcquire(/*exclusive=*/false, /*wait=*/true)) return false;
        m_sharedLockCount = 1;
        return true;
    } else {
        // Reentrant exclusive lock
        if (m_exclusiveLockCount > 0) {
            m_exclusiveLockCount++;
            return true;
        }
        // Promotion: release shared lock first (brief unlock window — documented limitation)
        if (m_sharedLockCount > 0) {
            flockRelease();
            m_sharedLockCount = 0;
        }
        if (!flockAcquire(/*exclusive=*/true, /*wait=*/true)) return false;
        m_exclusiveLockCount = 1;
        return true;
    }
}

bool FileLock::tryLock(LockType type) noexcept {
    if (type == LockType::SharedLock) {
        if (m_exclusiveLockCount > 0) {
            m_sharedLockCount++;
            return true;
        }
        if (m_sharedLockCount > 0) {
            m_sharedLockCount++;
            return true;
        }
        if (!flockAcquire(/*exclusive=*/false, /*wait=*/false)) return false;
        m_sharedLockCount = 1;
        return true;
    } else {
        if (m_exclusiveLockCount > 0) {
            m_exclusiveLockCount++;
            return true;
        }
        // Promotion attempt (non-blocking): release shared, try exclusive
        if (m_sharedLockCount > 0) {
            flockRelease();
            m_sharedLockCount = 0;
            if (!flockAcquire(/*exclusive=*/true, /*wait=*/false)) {
                // Failed to promote — re-acquire shared (best effort, non-blocking)
                flockAcquire(/*exclusive=*/false, /*wait=*/false);
                return false;
            }
            m_exclusiveLockCount = 1;
            return true;
        }
        if (!flockAcquire(/*exclusive=*/true, /*wait=*/false)) return false;
        m_exclusiveLockCount = 1;
        return true;
    }
}

void FileLock::unlock(LockType type) noexcept {
    if (type == LockType::SharedLock) {
        if (m_sharedLockCount == 0) return;
        m_sharedLockCount--;
        if (m_sharedLockCount == 0 && m_exclusiveLockCount == 0) {
            flockRelease();
        }
    } else {
        if (m_exclusiveLockCount == 0) return;
        m_exclusiveLockCount--;
        if (m_exclusiveLockCount == 0) {
            if (m_sharedLockCount > 0) {
                // Downgrade to shared (atomic with flock: LOCK_SH replaces LOCK_EX)
                flockAcquire(/*exclusive=*/false, /*wait=*/false);
            } else {
                flockRelease();
            }
        }
    }
}

} // namespace quickstore
