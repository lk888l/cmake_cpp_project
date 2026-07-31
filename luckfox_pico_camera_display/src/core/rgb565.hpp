#pragma once

#include <cstddef>
#include <cstdint>

namespace camera_display {

void encodeRgb565BigEndian(const std::uint16_t* source,
                          std::uint8_t* destination,
                          std::size_t pixelCount) noexcept;

} // namespace camera_display

