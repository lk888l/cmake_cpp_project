#include "runtime/render_policy.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#if defined(__linux__)
#include <unistd.h>
#endif

namespace runtime {
namespace {

constexpr std::uint64_t kMiBInKiB = 1024;

RenderPolicy defaultsFor(RenderProfile profile)
{
    switch (profile) {
    case RenderProfile::Low:
        return {
            .profile = RenderProfile::Low,
            .bufferLines = 40,
            .targetFps = 30,
            .refreshPeriodMs = 33,
            .extraHeapKiB = 32,
            .allowLargeObjectLayers = false,
            .animateSmallLayers = false,
        };
    case RenderProfile::Balanced:
        return {
            .profile = RenderProfile::Balanced,
            .bufferLines = 48,
            .targetFps = 40,
            .refreshPeriodMs = 25,
            .extraHeapKiB = 64,
            .allowLargeObjectLayers = false,
            .animateSmallLayers = true,
        };
    case RenderProfile::Quality:
        return {
            .profile = RenderProfile::Quality,
            .bufferLines = 60,
            .targetFps = 50,
            .refreshPeriodMs = 20,
            .extraHeapKiB = 128,
            .allowLargeObjectLayers = true,
            .animateSmallLayers = true,
        };
    case RenderProfile::Auto:
        break;
    }
    return defaultsFor(RenderProfile::Low);
}

RenderProfile autoProfile(const SystemResources& resources)
{
    if (!resources.onlineCpuCount || !resources.totalMemoryKiB
        || !resources.availableMemoryKiB || *resources.onlineCpuCount == 0) {
        return RenderProfile::Low;
    }

    if (*resources.onlineCpuCount <= 1
        || *resources.totalMemoryKiB <= 128 * kMiBInKiB
        || *resources.availableMemoryKiB <= 64 * kMiBInKiB) {
        return RenderProfile::Low;
    }
    if (*resources.onlineCpuCount <= 2
        || *resources.totalMemoryKiB <= 256 * kMiBInKiB
        || *resources.availableMemoryKiB <= 128 * kMiBInKiB) {
        return RenderProfile::Balanced;
    }
    return RenderProfile::Quality;
}

void readMemInfo(SystemResources& resources)
{
    std::ifstream input("/proc/meminfo");
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string key;
        std::uint64_t value{};
        if (!(fields >> key >> value)) {
            continue;
        }
        if (key == "MemTotal:") {
            resources.totalMemoryKiB = value;
        }
        else if (key == "MemAvailable:") {
            resources.availableMemoryKiB = value;
        }
    }
}

std::uint32_t refreshPeriod(std::uint16_t targetFps)
{
    return std::max<std::uint32_t>(
        1U, (1000U + static_cast<std::uint32_t>(targetFps) / 2U) / targetFps);
}

} // namespace

SystemResources detectSystemResources()
{
    SystemResources resources;
#if defined(__linux__)
    const long cpuCount = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (cpuCount > 0
        && static_cast<unsigned long>(cpuCount)
            <= std::numeric_limits<std::uint32_t>::max()) {
        resources.onlineCpuCount = static_cast<std::uint32_t>(cpuCount);
    }
#endif
    readMemInfo(resources);
    return resources;
}

RenderPolicy resolveRenderPolicy(const SystemResources& resources,
                                 const RenderOverrides& overrides)
{
    const auto resolvedProfile = overrides.profile == RenderProfile::Auto
        ? autoProfile(resources)
        : overrides.profile;
    auto policy = defaultsFor(resolvedProfile);
    if (overrides.bufferLines) {
        policy.bufferLines = *overrides.bufferLines;
    }
    if (overrides.targetFps) {
        policy.targetFps = *overrides.targetFps;
    }
    if (overrides.extraHeapKiB) {
        policy.extraHeapKiB = *overrides.extraHeapKiB;
    }
    policy.refreshPeriodMs = refreshPeriod(policy.targetFps);
    return policy;
}

std::optional<RenderProfile> parseRenderProfile(std::string_view text)
{
    if (text == "auto") return RenderProfile::Auto;
    if (text == "low") return RenderProfile::Low;
    if (text == "balanced") return RenderProfile::Balanced;
    if (text == "quality") return RenderProfile::Quality;
    return std::nullopt;
}

const char* toString(RenderProfile profile) noexcept
{
    switch (profile) {
    case RenderProfile::Auto: return "auto";
    case RenderProfile::Low: return "low";
    case RenderProfile::Balanced: return "balanced";
    case RenderProfile::Quality: return "quality";
    }
    return "unknown";
}

std::size_t estimateLargeLayerBytes(std::uint32_t width,
                                    std::uint32_t height,
                                    std::uint32_t bufferLines) noexcept
{
    constexpr std::size_t marginBytes = 8U * 1024U;
    const auto layerWidth = static_cast<std::size_t>(width) + 10U;
    const auto layerHeight = std::min<std::size_t>(
        static_cast<std::size_t>(height) + 10U, bufferLines);
    return layerWidth * layerHeight * 4U + marginBytes;
}

bool hasLargeLayerBudget(const RenderPolicy& policy,
                         std::size_t biggestFreeBlock,
                         std::uint32_t width,
                         std::uint32_t height) noexcept
{
    return policy.allowLargeObjectLayers
        && biggestFreeBlock
            >= estimateLargeLayerBytes(width, height, policy.bufferLines);
}

} // namespace runtime
