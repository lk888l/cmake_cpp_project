#include "core/rgb565.hpp"

namespace camera_display {

void encodeRgb565BigEndian(const std::uint16_t* source,
                          std::uint8_t* destination,
                          std::size_t pixelCount) noexcept
{
    for (std::size_t index = 0; index < pixelCount; ++index) {
        const std::uint16_t pixel = source[index];
        destination[index * 2U] = static_cast<std::uint8_t>(pixel >> 8U);
        destination[index * 2U + 1U] = static_cast<std::uint8_t>(pixel & 0xFFU);
    }
}

} // namespace camera_display

