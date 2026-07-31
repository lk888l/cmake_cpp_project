#pragma once

#include "core/metrics.hpp"
#include "core/recovery_policy.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace camera_display {

class StatusOverlay final {
public:
    static constexpr std::uint16_t kWidth = 240;
    static constexpr std::uint16_t kHeight = 14;

    StatusOverlay();

    bool render(const MetricsSnapshot& metrics,
                PipelineState state,
                std::uint32_t targetFps);

    [[nodiscard]] const std::uint16_t* pixels() const noexcept { return pixels_.data(); }
    [[nodiscard]] std::size_t pixelCount() const noexcept { return pixels_.size(); }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }

private:
    void drawCharacter(char character,
                       std::uint16_t originX,
                       std::uint16_t color) noexcept;

    std::vector<std::uint16_t> pixels_;
    std::string text_;
};

} // namespace camera_display

