#pragma once

#include <cstddef>
#include <cstdint>

namespace camera_display {

enum class IoStatus : std::uint8_t {
    Ok = 0,
    InvalidArgument,
    InvalidState,
    NotFound,
    PermissionDenied,
    Busy,
    Interrupted,
    Timeout,
    IoError,
};

[[nodiscard]] const char* toString(IoStatus status) noexcept;

struct SpiStatistics final {
    std::uint64_t transactions{};
    std::uint64_t bytes{};
    std::uint64_t errors{};
    std::uint64_t interrupted_retries{};
};

class SpiBus {
public:
    virtual ~SpiBus() = default;
    virtual IoStatus open() = 0;
    virtual IoStatus write(const std::uint8_t* bytes, std::size_t size) = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
    [[nodiscard]] virtual SpiStatistics statistics() const noexcept = 0;
};

class OutputPin {
public:
    virtual ~OutputPin() = default;
    virtual IoStatus request(bool initialHigh) = 0;
    virtual IoStatus set(bool high) = 0;
    virtual void release() noexcept = 0;
    [[nodiscard]] virtual bool isRequested() const noexcept = 0;
};

} // namespace camera_display
