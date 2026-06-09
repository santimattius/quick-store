#include "quickstore/mmap_region.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace quickstore {

std::optional<MmapRegion> MmapRegion::open(const std::string& path, AccessMode mode) {
    if (mode == AccessMode::ReadWrite) {
        int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644);
        if (fd < 0) return std::nullopt;

        struct stat st{};
        if (::fstat(fd, &st) != 0) {
            ::close(fd);
            return std::nullopt;
        }
        if (st.st_size <= 0) {
            ::close(fd);
            return std::nullopt;
        }

        size_t size = static_cast<size_t>(st.st_size);
        void* ptr = ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd);
            return std::nullopt;
        }

        return MmapRegion(fd, ptr, size, AccessMode::ReadWrite);
    }

    // ReadOnly path — unchanged from Fase 0.
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return std::nullopt;

    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        return std::nullopt;
    }
    if (st.st_size <= 0) {
        ::close(fd);
        return std::nullopt;
    }

    size_t size = static_cast<size_t>(st.st_size);
    void* ptr = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        ::close(fd);
        return std::nullopt;
    }

    return MmapRegion(fd, ptr, size, AccessMode::ReadOnly);
}

bool MmapRegion::grow(size_t newSize) noexcept {
    if (::ftruncate(m_fd, static_cast<off_t>(newSize)) != 0) return false;
    ::munmap(m_ptr, m_size);
    m_ptr = nullptr;
    m_size = 0;
    void* ptr = ::mmap(nullptr, newSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (ptr == MAP_FAILED) {
        return false;
    }
    m_ptr  = ptr;
    m_size = newSize;
    return true;
}

MmapRegion::~MmapRegion() { reset(); }

MmapRegion::MmapRegion(MmapRegion&& o) noexcept
    : m_fd(o.m_fd), m_ptr(o.m_ptr), m_size(o.m_size), m_mode(o.m_mode) {
    o.m_fd = -1; o.m_ptr = nullptr; o.m_size = 0;
}

MmapRegion& MmapRegion::operator=(MmapRegion&& o) noexcept {
    if (this != &o) {
        reset();
        m_fd = o.m_fd; m_ptr = o.m_ptr; m_size = o.m_size; m_mode = o.m_mode;
        o.m_fd = -1; o.m_ptr = nullptr; o.m_size = 0;
    }
    return *this;
}

void MmapRegion::reset() noexcept {
    if (m_ptr != nullptr && m_ptr != MAP_FAILED) ::munmap(m_ptr, m_size);
    if (m_fd != -1) ::close(m_fd);
    m_fd = -1; m_ptr = nullptr; m_size = 0;
}

} // namespace quickstore
