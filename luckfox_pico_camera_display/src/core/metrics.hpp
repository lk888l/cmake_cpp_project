#pragma once

#include <array>
#include <cstdint>

namespace camera_display {

struct MetricsSnapshot final {
    std::uint64_t captured{};
    std::uint64_t converted{};
    std::uint64_t displayed{};
    std::uint64_t dropped{};
    std::uint64_t capture_timeouts{};
    std::uint64_t rga_errors{};
    std::uint64_t spi_errors{};
    std::uint32_t fps_tenths{};
    std::uint32_t latency_p95_ms{};
    bool adaptive_degraded{};
};

class RuntimeMetrics final {
public:
    void onCaptured() noexcept { ++captured_; }
    void onConverted() noexcept { ++converted_; }
    void onDisplayed(std::uint32_t latencyMs) noexcept;
    void onDropped(std::uint64_t count = 1) noexcept { dropped_ += count; }
    void onCaptureTimeout() noexcept { ++capture_timeouts_; }
    void onRgaError() noexcept { ++rga_errors_; }
    void onSpiError() noexcept { ++spi_errors_; }

    [[nodiscard]] MetricsSnapshot snapshot(std::uint64_t elapsedMs) const noexcept;
    void resetInterval() noexcept;

private:
    static constexpr std::size_t kLatencyBuckets = 1001;
    std::uint64_t captured_{};
    std::uint64_t converted_{};
    std::uint64_t displayed_{};
    std::uint64_t dropped_{};
    std::uint64_t capture_timeouts_{};
    std::uint64_t rga_errors_{};
    std::uint64_t spi_errors_{};
    std::array<std::uint32_t, kLatencyBuckets> latency_histogram_{};
};

} // namespace camera_display
