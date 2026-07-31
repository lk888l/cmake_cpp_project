#include "core/layout.hpp"

#include <algorithm>
#include <cstdint>

namespace camera_display {

RenderLayout calculateLayout(std::uint16_t sourceWidth,
                             std::uint16_t sourceHeight,
                             std::uint16_t displayWidth,
                             std::uint16_t displayHeight,
                             AspectMode mode)
{
    RenderLayout result{{0, 0, sourceWidth, sourceHeight},
                        {0, 0, displayWidth, displayHeight}};
    if (sourceWidth == 0 || sourceHeight == 0
        || displayWidth == 0 || displayHeight == 0) {
        return {};
    }
    if (mode == AspectMode::Stretch) return result;

    const std::uint64_t sourceAspect =
        static_cast<std::uint64_t>(sourceWidth) * displayHeight;
    const std::uint64_t targetAspect =
        static_cast<std::uint64_t>(displayWidth) * sourceHeight;

    if (mode == AspectMode::Letterbox) {
        if (sourceAspect > targetAspect) {
            const auto scaledHeight = static_cast<std::uint16_t>(
                static_cast<std::uint64_t>(displayWidth) * sourceHeight / sourceWidth);
            result.destination.height = std::max<std::uint16_t>(1, scaledHeight);
            result.destination.y = static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(displayHeight)
                 - result.destination.height) / 2U);
        }
        else {
            const auto scaledWidth = static_cast<std::uint16_t>(
                static_cast<std::uint64_t>(displayHeight) * sourceWidth / sourceHeight);
            result.destination.width = std::max<std::uint16_t>(1, scaledWidth);
            result.destination.x = static_cast<std::uint16_t>(
                (static_cast<std::uint32_t>(displayWidth)
                 - result.destination.width) / 2U);
        }
        return result;
    }

    if (sourceAspect > targetAspect) {
        const auto cropWidth = static_cast<std::uint16_t>(
            static_cast<std::uint64_t>(sourceHeight) * displayWidth / displayHeight);
        result.source_crop.width = static_cast<std::uint16_t>(cropWidth & ~1U);
        result.source_crop.x = static_cast<std::uint16_t>(
            ((static_cast<std::uint32_t>(sourceWidth)
              - result.source_crop.width) / 2U) & ~1U);
    }
    else {
        const auto cropHeight = static_cast<std::uint16_t>(
            static_cast<std::uint64_t>(sourceWidth) * displayHeight / displayWidth);
        result.source_crop.height = static_cast<std::uint16_t>(cropHeight & ~1U);
        result.source_crop.y = static_cast<std::uint16_t>(
            ((static_cast<std::uint32_t>(sourceHeight)
              - result.source_crop.height) / 2U) & ~1U);
    }
    return result;
}

} // namespace camera_display
