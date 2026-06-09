#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace quickstore {

enum class AccessMode { ReadOnly, ReadWrite };

class MmapRegion {
public:
    [[nodiscard]] static std::optional<MmapRegion>
    open(const std::string& path, AccessMode mode);

    ~MmapRegion();

    MmapRegion(MmapRegion&& other) noexcept;
    MmapRegion& operator=(MmapRegion&& other) noexcept;

    MmapRegion(const MmapRegion&)            = delete;
    MmapRegion& operator=(const MmapRegion&) = delete;

    [[nodiscard]] const uint8_t* base() const noexcept {
        return reinterpret_cast<const uint8_t*>(m_ptr);
    }
    [[nodiscard]] size_t size() const noexcept { return m_size; }

    // WHY: m_fd is kept open after mmap so Fase 1 can ftruncate(m_fd, newSize) + re-mmap
    // for full write-back without reopening the file descriptor.

    // Returns a writable pointer into the mapping.
    // PRECONDITION: mode == ReadWrite. Calling on a ReadOnly region returns a pointer
    // into PROT_READ memory — writing through it will SIGSEGV.
    [[nodiscard]] uint8_t* mutableBase() noexcept {
        return static_cast<uint8_t*>(m_ptr);
    }

    // Returns the mapped size (same as size()).
    [[nodiscard]] size_t capacity() const noexcept { return m_size; }

    // Grows the mapping to newSize via ftruncate → munmap → mmap.
    // On mmap failure after munmap, sets m_ptr=nullptr, m_size=0 and returns false.
    // On success, updates m_ptr and m_size and returns true.
    // WARNING: all previously obtained raw pointers (including mutableBase()) are
    // DANGLING after a successful grow(). Re-fetch mutableBase() after every call.
    [[nodiscard]] bool grow(size_t newSize) noexcept;

private:
    MmapRegion(int fd, void* ptr, size_t size, AccessMode mode) noexcept
        : m_fd(fd), m_ptr(ptr), m_size(size), m_mode(mode) {}

    void reset() noexcept;

    int        m_fd   = -1;
    void*      m_ptr  = nullptr;
    size_t     m_size = 0;
    AccessMode m_mode = AccessMode::ReadOnly;
};

} // namespace quickstore
