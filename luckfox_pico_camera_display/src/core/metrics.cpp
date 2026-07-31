#include "core/metrics.hpp"

#include <algorithm>
#include <cstddef>

namespace camera_display {

void RuntimeMetrics::onDisplayed(std::uint32_t latencyMs) noexcept
{
    ++displayed_;
    const std::size_t bucket = std::min<std::size_t>(
        latencyMs, latency_histogram_.size() - 1U);
    ++latency_histogram_[bucket];
}

MetricsSnapshot RuntimeMetrics::snapshot(std::uint64_t elapsedMs) const noexcept
{
    MetricsSnapshot result;
    result.captured = captured_;
    result.converted = converted_;
    result.displayed = displayed_;
    result.dropped = dropped_;
    result.capture_timeouts = capture_timeouts_;
    result.rga_errors = rga_errors_;
    result.spi_errors = spi_errors_;
    if (elapsedMs != 0) {
        result.fps_tenths = static_cast<std::uint32_t>(
            (displayed_ * 10'000ULL + elapsedMs / 2ULL) / elapsedMs);
    }

    if (displayed_ != 0) {
        const std::uint64_t target = (displayed_ * 95ULL + 99ULL) / 100ULL;
        std::uint64_t cumulative{};
        for (std::size_t index = 0; index < latency_histogram_.size(); ++index) {
            cumulative += latency_histogram_[index];
            if (cumulative >= target) {
                result.latency_p95_ms = static_cast<std::uint32_t>(index);
                break;
            }
        }
    }
    return result;
}

void RuntimeMetrics::resetInterval() noexcept
{
    captured_ = 0;
    converted_ = 0;
    displayed_ = 0;
    dropped_ = 0;
    capture_timeouts_ = 0;
    rga_errors_ = 0;
    spi_errors_ = 0;
    latency_histogram_.fill(0);
}

} // namespace camera_display

