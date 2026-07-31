#include "core/status_overlay.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

namespace camera_display {
namespace {

using Glyph = std::array<std::uint8_t, 5>;

Glyph glyph(char character) noexcept
{
    switch (character) {
    case '0': return {{0x3E, 0x51, 0x49, 0x45, 0x3E}};
    case '1': return {{0x00, 0x42, 0x7F, 0x40, 0x00}};
    case '2': return {{0x42, 0x61, 0x51, 0x49, 0x46}};
    case '3': return {{0x21, 0x41, 0x45, 0x4B, 0x31}};
    case '4': return {{0x18, 0x14, 0x12, 0x7F, 0x10}};
    case '5': return {{0x27, 0x45, 0x45, 0x45, 0x39}};
    case '6': return {{0x3C, 0x4A, 0x49, 0x49, 0x30}};
    case '7': return {{0x01, 0x71, 0x09, 0x05, 0x03}};
    case '8': return {{0x36, 0x49, 0x49, 0x49, 0x36}};
    case '9': return {{0x06, 0x49, 0x49, 0x29, 0x1E}};
    case 'A': return {{0x7E, 0x11, 0x11, 0x11, 0x7E}};
    case 'D': return {{0x7F, 0x41, 0x41, 0x22, 0x1C}};
    case 'F': return {{0x7F, 0x09, 0x09, 0x09, 0x01}};
    case 'I': return {{0x00, 0x41, 0x7F, 0x41, 0x00}};
    case 'L': return {{0x7F, 0x40, 0x40, 0x40, 0x40}};
    case 'N': return {{0x7F, 0x02, 0x04, 0x08, 0x7F}};
    case 'O': return {{0x3E, 0x41, 0x41, 0x41, 0x3E}};
    case 'P': return {{0x7F, 0x09, 0x09, 0x09, 0x06}};
    case 'R': return {{0x7F, 0x09, 0x19, 0x29, 0x46}};
    case 'S': return {{0x46, 0x49, 0x49, 0x49, 0x31}};
    case 'T': return {{0x01, 0x01, 0x7F, 0x01, 0x01}};
    case 'W': return {{0x3F, 0x40, 0x38, 0x40, 0x3F}};
    case '.': return {{0x00, 0x60, 0x60, 0x00, 0x00}};
    case ':': return {{0x00, 0x36, 0x36, 0x00, 0x00}};
    case '-': return {{0x08, 0x08, 0x08, 0x08, 0x08}};
    case ' ': return {{0, 0, 0, 0, 0}};
    default: return {{0x02, 0x01, 0x51, 0x09, 0x06}};
    }
}

std::uint16_t colorFor(PipelineState state, std::uint32_t fpsTenths,
                       std::uint32_t targetFps) noexcept
{
    if (state == PipelineState::Failed) return 0xF800;
    if (state == PipelineState::Recovering || state == PipelineState::Starting) {
        return 0xFFE0;
    }
    if (fpsTenths + 5U < targetFps * 10U) return 0xFFE0;
    return 0xFFFF;
}

} // namespace

StatusOverlay::StatusOverlay()
    : pixels_(static_cast<std::size_t>(kWidth) * kHeight, 0)
{
}

bool StatusOverlay::render(const MetricsSnapshot& metrics,
                           PipelineState state,
                           std::uint32_t targetFps)
{
    char buffer[32]{};
    const char stateLetter =
        metrics.adaptive_degraded ? 'A'
        : state == PipelineState::Running ? 'R'
        : state == PipelineState::Recovering ? 'W'
        : state == PipelineState::Failed ? 'F' : 'S';
    std::snprintf(buffer, sizeof(buffer), "%2u.%1uF %3uL D%03llu %c",
                  metrics.fps_tenths / 10U,
                  metrics.fps_tenths % 10U,
                  std::min<std::uint32_t>(999, metrics.latency_p95_ms),
                  static_cast<unsigned long long>(
                      std::min<std::uint64_t>(999, metrics.dropped)),
                  stateLetter);
    const std::string next{buffer};
    if (next == text_) return false;
    text_ = next;
    std::fill(pixels_.begin(), pixels_.end(), 0);
    const std::uint16_t color = colorFor(state, metrics.fps_tenths, targetFps);
    std::uint16_t x{};
    for (char character : text_) {
        if (x + 10U > kWidth) break;
        drawCharacter(character, x, color);
        x = static_cast<std::uint16_t>(x + 12U);
    }
    return true;
}

void StatusOverlay::drawCharacter(char character,
                                  std::uint16_t originX,
                                  std::uint16_t color) noexcept
{
    const Glyph columns = glyph(character);
    for (std::size_t column = 0; column < columns.size(); ++column) {
        for (std::uint16_t row = 0; row < 7; ++row) {
            if ((columns[column] & (1U << row)) == 0) continue;
            for (std::uint16_t dy = 0; dy < 2; ++dy) {
                for (std::uint16_t dx = 0; dx < 2; ++dx) {
                    const std::uint16_t x =
                        static_cast<std::uint16_t>(
                            static_cast<std::size_t>(originX) + column * 2U + dx);
                    const std::uint16_t y =
                        static_cast<std::uint16_t>(row * 2U + dy);
                    pixels_[static_cast<std::size_t>(y) * kWidth + x] = color;
                }
            }
        }
    }
}

} // namespace camera_display
