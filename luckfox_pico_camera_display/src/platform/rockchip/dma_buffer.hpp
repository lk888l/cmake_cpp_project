#pragma once

#include <cstddef>
#include <string>

namespace camera_display {

class DmaBuffer final {
public:
    DmaBuffer() = default;
    ~DmaBuffer();
    DmaBuffer(const DmaBuffer&) = delete;
    DmaBuffer& operator=(const DmaBuffer&) = delete;
    DmaBuffer(DmaBuffer&& other) noexcept;
    DmaBuffer& operator=(DmaBuffer&& other) noexcept;

    bool allocate(const std::string& heapPath, std::size_t byteSize,
                  std::string& error);
    bool beginCpuRead(std::string& error) noexcept;
    bool endCpuRead(std::string& error) noexcept;
    void reset() noexcept;

    [[nodiscard]] int fd() const noexcept { return fd_; }
    [[nodiscard]] void* data() const noexcept { return mapping_; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

private:
    bool synchronize(unsigned long flags, std::string& error) noexcept;

    int fd_{-1};
    void* mapping_{};
    std::size_t size_{};
    bool cpu_read_active_{};
};

} // namespace camera_display
