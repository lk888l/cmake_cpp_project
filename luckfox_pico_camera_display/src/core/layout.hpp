#pragma once

#include "core/config.hpp"

#include <cstdint>

namespace camera_display {

struct Rect final {
    std::uint16_t x{};
    std::uint16_t y{};
    std::uint16_t width{};
    std::uint16_t height{};
};

struct RenderLayout final {
    Rect source_crop;
    Rect destination;
};

[[nodiscard]] RenderLayout calculateLayout(
    std::uint16_t sourceWidth,
    std::uint16_t sourceHeight,
    std::uint16_t displayWidth,
    std::uint16_t displayHeight,
    AspectMode mode);

} // namespace camera_display

