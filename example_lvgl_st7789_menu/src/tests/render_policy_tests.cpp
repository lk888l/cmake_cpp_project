#include "runtime/render_policy.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

runtime::SystemResources resources(std::uint32_t cpus,
                                   std::uint64_t totalMiB,
                                   std::uint64_t availableMiB)
{
    return {
        .onlineCpuCount = cpus,
        .totalMemoryKiB = totalMiB * 1024U,
        .availableMemoryKiB = availableMiB * 1024U,
    };
}

void automaticThresholds()
{
    check(runtime::resolveRenderPolicy(resources(1, 512, 400)).profile
              == runtime::RenderProfile::Low,
          "single CPU selects low");
    check(runtime::resolveRenderPolicy(resources(4, 128, 100)).profile
              == runtime::RenderProfile::Low,
          "128 MiB total selects low at inclusive boundary");
    check(runtime::resolveRenderPolicy(resources(4, 512, 64)).profile
              == runtime::RenderProfile::Low,
          "64 MiB available selects low at inclusive boundary");
    check(runtime::resolveRenderPolicy(resources(2, 512, 400)).profile
              == runtime::RenderProfile::Balanced,
          "two CPUs select balanced");
    check(runtime::resolveRenderPolicy(resources(4, 256, 200)).profile
              == runtime::RenderProfile::Balanced,
          "256 MiB total selects balanced at inclusive boundary");
    check(runtime::resolveRenderPolicy(resources(4, 512, 128)).profile
              == runtime::RenderProfile::Balanced,
          "128 MiB available selects balanced at inclusive boundary");
    check(runtime::resolveRenderPolicy(resources(4, 512, 300)).profile
              == runtime::RenderProfile::Quality,
          "larger system selects quality");

    runtime::SystemResources incomplete;
    incomplete.onlineCpuCount = 4;
    check(runtime::resolveRenderPolicy(incomplete).profile
              == runtime::RenderProfile::Low,
          "missing memory data falls back to low");
}

void defaultsAndOverrides()
{
    const auto low = runtime::resolveRenderPolicy(resources(1, 128, 64));
    check(low.bufferLines == 40 && low.targetFps == 30
              && low.refreshPeriodMs == 33 && low.extraHeapKiB == 32,
          "low defaults");
    check(!low.allowLargeObjectLayers && !low.animateSmallLayers,
          "low disables layer effects");

    const auto balanced = runtime::resolveRenderPolicy(
        resources(8, 1024, 800),
        {
            .profile = runtime::RenderProfile::Balanced,
            .bufferLines = std::nullopt,
            .targetFps = std::nullopt,
            .extraHeapKiB = std::nullopt,
        });
    check(balanced.bufferLines == 48 && balanced.targetFps == 40
              && balanced.refreshPeriodMs == 25 && balanced.extraHeapKiB == 64,
          "balanced forced defaults");

    const auto quality = runtime::resolveRenderPolicy(
        {},
        {
            .profile = runtime::RenderProfile::Quality,
            .bufferLines = 7,
            .targetFps = 40,
            .extraHeapKiB = 96,
        });
    check(quality.profile == runtime::RenderProfile::Quality,
          "forced profile overrides missing detection");
    check(quality.bufferLines == 7 && quality.targetFps == 40
              && quality.refreshPeriodMs == 25 && quality.extraHeapKiB == 96,
          "explicit values override profile defaults");
    check(quality.allowLargeObjectLayers && quality.animateSmallLayers,
          "quality permits layer effects");
}

void profileParsingAndLayerBudget()
{
    check(runtime::parseRenderProfile("auto") == runtime::RenderProfile::Auto,
          "parse auto");
    check(runtime::parseRenderProfile("balanced") == runtime::RenderProfile::Balanced,
          "parse balanced");
    check(!runtime::parseRenderProfile("fast"), "reject invalid profile");

    auto quality = runtime::resolveRenderPolicy(
        resources(4, 512, 300),
        {
            .profile = runtime::RenderProfile::Quality,
            .bufferLines = std::nullopt,
            .targetFps = std::nullopt,
            .extraHeapKiB = std::nullopt,
        });
    const auto expected = (208U + 10U) * 60U * 4U + 8U * 1024U;
    check(runtime::estimateLargeLayerBytes(208, 112, quality.bufferLines) == expected,
          "layer estimate follows bounded formula");
    check(!runtime::hasLargeLayerBudget(quality, expected - 1U, 208, 112),
          "largest block below estimate rejects layers");
    check(runtime::hasLargeLayerBudget(quality, expected, 208, 112),
          "largest block at estimate permits layers");

    const auto low = runtime::resolveRenderPolicy(resources(1, 128, 64));
    check(!runtime::hasLargeLayerBudget(low, expected * 10U, 208, 112),
          "low profile rejects layers regardless of memory");
}

} // namespace

int main()
{
    automaticThresholds();
    defaultsAndOverrides();
    profileParsingAndLayerBudget();
    std::cout << "render policy tests passed\n";
}
