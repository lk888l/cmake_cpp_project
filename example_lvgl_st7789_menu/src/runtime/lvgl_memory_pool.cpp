#include "runtime/lvgl_memory_pool.hpp"

#include "lvgl.h"

namespace runtime {

bool LvglMemoryPool::add(std::size_t bytes)
{
    if (pool_ != nullptr || !storage_.empty()) {
        return false;
    }
    if (bytes == 0) {
        return true;
    }

    const auto units = (bytes + sizeof(std::max_align_t) - 1U)
        / sizeof(std::max_align_t);
    storage_.resize(units);
    pool_ = lv_mem_add_pool(storage_.data(), storage_.size() * sizeof(std::max_align_t));
    if (pool_ == nullptr) {
        storage_.clear();
        storage_.shrink_to_fit();
        return false;
    }
    return true;
}

std::size_t LvglMemoryPool::sizeBytes() const noexcept
{
    return storage_.size() * sizeof(std::max_align_t);
}

} // namespace runtime
