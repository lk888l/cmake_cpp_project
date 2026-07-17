#pragma once

#include <cstddef>
#include <vector>

namespace runtime {

class LvglMemoryPool final {
public:
    LvglMemoryPool() = default;
    ~LvglMemoryPool() = default;

    LvglMemoryPool(const LvglMemoryPool&) = delete;
    LvglMemoryPool& operator=(const LvglMemoryPool&) = delete;

    [[nodiscard]] bool add(std::size_t bytes);
    [[nodiscard]] std::size_t sizeBytes() const noexcept;

private:
    std::vector<std::max_align_t> storage_;
    void* pool_{};
};

} // namespace runtime
