#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace runtime {

enum class RenderProfile : std::uint8_t {
    Auto,
    Low,
    Balanced,
    Quality,
};

struct SystemResources final {
    std::optional<std::uint32_t> onlineCpuCount;
    std::optional<std::uint64_t> totalMemoryKiB;
    std::optional<std::uint64_t> availableMemoryKiB;
};

struct RenderOverrides final {
    RenderProfile profile{RenderProfile::Auto};
    std::optional<std::uint16_t> bufferLines;
    std::optional<std::uint16_t> targetFps;
    std::optional<std::uint32_t> extraHeapKiB;
};

struct RenderPolicy final {
    RenderProfile profile{RenderProfile::Low};
    std::uint16_t bufferLines{24};
    std::uint16_t targetFps{20};
    std::uint32_t refreshPeriodMs{50};
    std::uint32_t extraHeapKiB{32};
    bool allowLargeObjectLayers{false};
    bool animateSmallLayers{false};
};

[[nodiscard]] SystemResources detectSystemResources();
[[nodiscard]] RenderPolicy resolveRenderPolicy(
    const SystemResources& resources, const RenderOverrides& overrides = {});
[[nodiscard]] std::optional<RenderProfile> parseRenderProfile(std::string_view text);
[[nodiscard]] const char* toString(RenderProfile profile) noexcept;

[[nodiscard]] std::size_t estimateLargeLayerBytes(
    std::uint32_t width, std::uint32_t height, std::uint32_t bufferLines) noexcept;
[[nodiscard]] bool hasLargeLayerBudget(const RenderPolicy& policy,
                                       std::size_t biggestFreeBlock,
                                       std::uint32_t width,
                                       std::uint32_t height) noexcept;

} // namespace runtime
