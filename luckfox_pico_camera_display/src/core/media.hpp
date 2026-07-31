#pragma once

#include "core/layout.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace camera_display {

enum class MediaStatus : std::uint8_t {
    Ok = 0,
    InvalidArgument,
    InvalidState,
    NotFound,
    Busy,
    Timeout,
    Unsupported,
    IoError,
};

[[nodiscard]] const char* toString(MediaStatus status) noexcept;

struct CapturedFrame final {
    int dma_fd{-1};
    void* virtual_address{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t horizontal_stride{};
    std::uint32_t vertical_stride{};
    std::uint64_t sequence{};
    std::uint64_t sensor_timestamp_us{};
    std::chrono::steady_clock::time_point acquired_at{};
    std::uintptr_t native_handle{};
};

struct ConvertedFrame final {
    int dma_fd{-1};
    std::uint16_t* pixels{};
    std::uint16_t width{};
    std::uint16_t height{};
    std::size_t byte_size{};
};

class FrameSource {
public:
    virtual ~FrameSource() = default;
    virtual MediaStatus start() = 0;
    virtual MediaStatus acquire(CapturedFrame& frame,
                                std::chrono::milliseconds timeout) = 0;
    virtual void release(CapturedFrame& frame) noexcept = 0;
    virtual void stop() noexcept = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

class HardwareConverter {
public:
    virtual ~HardwareConverter() = default;
    virtual MediaStatus start() = 0;
    virtual MediaStatus convert(const CapturedFrame& source,
                                const Rect& sourceCrop,
                                std::size_t outputSlot) = 0;
    [[nodiscard]] virtual ConvertedFrame output(std::size_t slot) noexcept = 0;
    virtual void stop() noexcept = 0;
    [[nodiscard]] virtual std::string description() const = 0;
};

} // namespace camera_display
