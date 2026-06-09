#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace quickstore {

class MMBuffer {
public:
    MMBuffer() = default;

    // Copies [src, src+len) into owned storage.
    MMBuffer(const uint8_t* src, size_t len) : m_data(src, src + len) {}

    [[nodiscard]] const uint8_t* data() const noexcept { return m_data.data(); }
    [[nodiscard]] size_t         size() const noexcept { return m_data.size(); }
    [[nodiscard]] bool           empty() const noexcept { return m_data.empty(); }

private:
    std::vector<uint8_t> m_data;
};

} // namespace quickstore
