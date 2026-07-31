#include "app/application.hpp"

#include "core/frame_mailbox.hpp"
#include "core/layout.hpp"
#include "core/metrics.hpp"
#include "core/recovery_policy.hpp"
#include "core/status_overlay.hpp"
#include "drivers/st7789.hpp"
#include "platform/linux/gpio_output.hpp"
#include "platform/linux/spi_bus.hpp"
#include "platform/rockchip/media_pipeline.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <unistd.h>

namespace camera_display {
namespace {

using namespace std::chrono_literals;

class DisplayStack final {
public:
    explicit DisplayStack(const DisplayConfig& config)
        : spi(config.spi_device, config.spi_hz, config.spi_chunk_bytes),
          dc(config.gpio_chip, static_cast<unsigned int>(config.dc_line),
             "camera-display-dc"),
          reset(config.gpio_chip, static_cast<unsigned int>(config.reset_line),
                "camera-display-reset"),
          backlight(config.gpio_chip,
                    static_cast<unsigned int>(config.backlight_line),
                    "camera-display-backlight"),
          panel(spi, dc, reset, backlight, config)
    {
    }

    ExitCode initialize()
    {
        IoStatus status = spi.open();
        if (status != IoStatus::Ok) {
            std::cerr << "SPI open failed: " << toString(status) << '\n';
            return ExitCode::SpiFailed;
        }
        if ((status = dc.request(false)) != IoStatus::Ok
            || (status = reset.request(true)) != IoStatus::Ok
            || (status = backlight.request(false)) != IoStatus::Ok) {
            std::cerr << "GPIO request failed: " << toString(status) << '\n';
            return ExitCode::GpioFailed;
        }
        if ((status = panel.initialize()) != IoStatus::Ok) {
            std::cerr << "ST7789 initialization failed: " << toString(status) << '\n';
            return ExitCode::SpiFailed;
        }
        return ExitCode::Success;
    }

    LinuxSpiBus spi;
    LinuxGpioOutput dc;
    LinuxGpioOutput reset;
    LinuxGpioOutput backlight;
    St7789 panel;
};

enum class CycleFault : int {
    None,
    Camera,
    Rga,
    Spi,
};

std::uint32_t elapsedMilliseconds(
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end) noexcept
{
    const auto value =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (value <= 0) return 0;
    return static_cast<std::uint32_t>(
        std::min<std::int64_t>(value, UINT32_MAX));
}

void writeOperationalRecord(
    int descriptor, const char* bytes, std::size_t size) noexcept
{
    std::size_t offset{};
    while (offset < size) {
        const ssize_t written = ::write(descriptor, bytes + offset, size - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return;
        offset += static_cast<std::size_t>(written);
    }
}

ExitCode mediaExitCode(MediaStage stage)
{
    return stage == MediaStage::Rga ? ExitCode::RgaFailed : ExitCode::CameraFailed;
}

} // namespace

Application::Application(AppConfig config, std::atomic_bool& stopRequested)
    : config_(std::move(config)),
      stop_requested_(stopRequested)
{
}

ExitCode Application::selfTestLcd()
{
    DisplayStack display{config_.display};
    const ExitCode initialized = display.initialize();
    if (initialized != ExitCode::Success) return initialized;

    constexpr std::array<std::uint16_t, 8> colors{{
        0xFFFF, 0xFFE0, 0x07FF, 0x07E0,
        0xF81F, 0xF800, 0x001F, 0x0000}};
    std::vector<std::uint16_t> pattern(
        static_cast<std::size_t>(config_.display.width) * config_.display.height);
    for (std::uint16_t y = 0; y < config_.display.height; ++y) {
        for (std::uint16_t x = 0; x < config_.display.width; ++x) {
            const std::size_t band =
                std::min<std::size_t>(colors.size() - 1U, x / 30U);
            pattern[static_cast<std::size_t>(y) * config_.display.width + x] =
                colors[band];
        }
    }
    // Direction markers: red top-left, green top-right, blue bottom-left,
    // white bottom-right. They also make RGB/BGR and byte-order errors obvious.
    for (std::uint16_t y = 0; y < 20; ++y) {
        for (std::uint16_t x = 0; x < 20; ++x) {
            pattern[static_cast<std::size_t>(y) * 240U + x] = 0xF800;
            pattern[static_cast<std::size_t>(y) * 240U + (239U - x)] = 0x07E0;
            pattern[static_cast<std::size_t>(239U - y) * 240U + x] = 0x001F;
            pattern[static_cast<std::size_t>(239U - y) * 240U + (239U - x)] =
                0xFFFF;
        }
    }

    const auto started = std::chrono::steady_clock::now();
    const IoStatus status =
        display.panel.writeRectangle({0, 0, 240, 240}, pattern.data(), pattern.size());
    const auto finished = std::chrono::steady_clock::now();
    if (status != IoStatus::Ok) {
        std::cerr << "LCD self-test transfer failed: " << toString(status) << '\n';
        return ExitCode::SpiFailed;
    }
    const auto microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(finished - started)
            .count();
    const double mibPerSecond = microseconds > 0
        ? (static_cast<double>(pattern.size() * 2U) * 1'000'000.0
           / static_cast<double>(microseconds) / (1024.0 * 1024.0))
        : 0.0;
    const SpiStatistics statistics = display.spi.statistics();
    std::cout << "LCD self-test passed: transfer=" << microseconds
              << " us, throughput=" << mibPerSecond
              << " MiB/s, SPI errors=" << statistics.errors << '\n';
    std::cout << "Expected corners: TL red, TR green, BL blue, BR white\n";
    for (int index = 0; index < 30 && !stop_requested_.load(); ++index) {
        std::this_thread::sleep_for(100ms);
    }
    return ExitCode::Success;
}

ExitCode Application::run()
{
    DisplayStack display{config_.display};
    const ExitCode initialized = display.initialize();
    if (initialized != ExitCode::Success) return initialized;

    RecoveryPolicy recovery{config_.runtime.maximum_restarts};
    ExitCode lastFailure = ExitCode::RuntimeFailed;
    const RenderLayout layout = calculateLayout(
        static_cast<std::uint16_t>(config_.camera.width),
        static_cast<std::uint16_t>(config_.camera.height),
        config_.display.width, config_.display.height,
        config_.render.aspect);
    StatusOverlay lifecycleOverlay;
    const auto showLifecycle = [&](PipelineState state) {
        if (!config_.render.osd_enabled) return IoStatus::Ok;
        MetricsSnapshot empty;
        (void)lifecycleOverlay.render(
            empty, state, config_.camera.target_fps);
        return display.panel.writeRectangle(
            {0, 19, StatusOverlay::kWidth, StatusOverlay::kHeight},
            lifecycleOverlay.pixels(), lifecycleOverlay.pixelCount());
    };
    if (showLifecycle(PipelineState::Starting) != IoStatus::Ok) {
        return ExitCode::SpiFailed;
    }

    while (!stop_requested_.load()) {
        RockchipMediaPipeline media{
            config_.camera, layout.destination.width, layout.destination.height};
        const MediaStatus startStatus = media.start();
        if (startStatus != MediaStatus::Ok) {
            lastFailure = mediaExitCode(media.failedStage());
            std::cerr << "Media start failed at stage "
                      << static_cast<int>(media.failedStage()) << ": "
                      << media.lastError() << '\n';
        }
        else {
            recovery.markRunning();
            std::cout << "Pipeline running: " << media.description()
                      << std::endl;

            LatestFrameMailbox<2> mailbox;
            RuntimeMetrics metrics;
            std::mutex metricsMutex;
            std::atomic<CycleFault> fault{CycleFault::None};
            std::atomic<std::uint32_t> desiredFps{config_.camera.target_fps};
            std::atomic<std::uint64_t> pressureDrops{};
            std::atomic<PipelineState> state{PipelineState::Running};
            std::thread capture([&] {
                std::uint32_t consecutiveTimeouts{};
                std::uint32_t consecutiveRgaErrors{};
                std::uint32_t rateAccumulator{};
                while (!stop_requested_.load()
                       && fault.load() == CycleFault::None) {
                    CapturedFrame input;
                    const MediaStatus acquired = media.acquire(
                        input,
                        std::chrono::milliseconds{
                            config_.camera.frame_timeout_ms});
                    if (acquired == MediaStatus::Timeout) {
                        {
                            std::lock_guard<std::mutex> lock(metricsMutex);
                            metrics.onCaptureTimeout();
                        }
                        if (++consecutiveTimeouts >= 3U) {
                            fault.store(CycleFault::Camera);
                            mailbox.stop();
                        }
                        continue;
                    }
                    if (acquired != MediaStatus::Ok) {
                        fault.store(CycleFault::Camera);
                        mailbox.stop();
                        continue;
                    }
                    consecutiveTimeouts = 0;
                    {
                        std::lock_guard<std::mutex> lock(metricsMutex);
                        metrics.onCaptured();
                    }

                    const std::uint32_t fps = desiredFps.load();
                    rateAccumulator += fps;
                    if (rateAccumulator < config_.camera.target_fps) {
                        media.release(input);
                        std::lock_guard<std::mutex> lock(metricsMutex);
                        metrics.onDropped();
                        continue;
                    }
                    rateAccumulator -= config_.camera.target_fps;

                    const std::uint64_t overwrittenBefore =
                        mailbox.overwrittenCount();
                    const auto slot = mailbox.acquireForWrite();
                    if (!slot) {
                        media.release(input);
                        std::lock_guard<std::mutex> lock(metricsMutex);
                        metrics.onDropped();
                        pressureDrops.fetch_add(1);
                        continue;
                    }
                    const MediaStatus converted =
                        media.convert(input, layout.source_crop, *slot);
                    const FrameMetadata metadata{
                        input.sequence,
                        input.sensor_timestamp_us,
                        input.acquired_at};
                    media.release(input);
                    if (converted != MediaStatus::Ok) {
                        mailbox.cancelWrite(*slot);
                        {
                            std::lock_guard<std::mutex> lock(metricsMutex);
                            metrics.onRgaError();
                        }
                        if (++consecutiveRgaErrors >= 3U) {
                            fault.store(CycleFault::Rga);
                            mailbox.stop();
                        }
                        continue;
                    }
                    consecutiveRgaErrors = 0;
                    {
                        std::lock_guard<std::mutex> lock(metricsMutex);
                        metrics.onConverted();
                    }
                    if (!mailbox.publish(*slot, metadata)) continue;
                    const std::uint64_t overwrittenAfter =
                        mailbox.overwrittenCount();
                    if (overwrittenAfter > overwrittenBefore) {
                        std::lock_guard<std::mutex> lock(metricsMutex);
                        metrics.onDropped(overwrittenAfter - overwrittenBefore);
                        pressureDrops.fetch_add(
                            overwrittenAfter - overwrittenBefore);
                    }
                }
            });

            std::thread presenter([&] {
                StatusOverlay overlay;
                std::array<char, 160> operationalRecord{};
                std::uint64_t cumulativeDropped{};
                std::uint32_t unhealthySeconds{};
                std::uint32_t healthySeconds{};
                bool panelResetAttempted{};
                auto intervalStart = std::chrono::steady_clock::now();
                auto nextStats = intervalStart
                    + std::chrono::milliseconds{config_.runtime.stats_period_ms};
                auto nextOsd = intervalStart
                    + std::chrono::milliseconds{config_.render.osd_period_ms};
                while (!stop_requested_.load()
                       && fault.load() == CycleFault::None) {
                    const auto ticket = mailbox.waitForRead(20ms);
                    if (ticket) {
                        if (!media.beginCpuRead(ticket->slot)) {
                            fault.store(CycleFault::Rga);
                            (void)mailbox.releaseRead(ticket->slot);
                            mailbox.stop();
                            break;
                        }
                        ConvertedFrame frame = media.output(ticket->slot);
                        IoStatus status = display.panel.writeRectangle(
                            layout.destination, frame.pixels,
                            static_cast<std::size_t>(frame.width) * frame.height);
                        if (status != IoStatus::Ok && !panelResetAttempted) {
                            panelResetAttempted = true;
                            {
                                std::lock_guard<std::mutex> lock(metricsMutex);
                                metrics.onSpiError();
                            }
                            status = display.panel.resetAndInitialize();
                            if (status == IoStatus::Ok) {
                                status = display.panel.writeRectangle(
                                    layout.destination, frame.pixels,
                                    static_cast<std::size_t>(frame.width)
                                        * frame.height);
                            }
                        }
                        if (!media.endCpuRead(ticket->slot)) {
                            fault.store(CycleFault::Rga);
                            mailbox.stop();
                        }
                        if (status != IoStatus::Ok) {
                            {
                                std::lock_guard<std::mutex> lock(metricsMutex);
                                metrics.onSpiError();
                            }
                            fault.store(CycleFault::Spi);
                            mailbox.stop();
                        }
                        else {
                            const auto done = std::chrono::steady_clock::now();
                            std::lock_guard<std::mutex> lock(metricsMutex);
                            metrics.onDisplayed(
                                elapsedMilliseconds(ticket->metadata.ready_at, done));
                        }
                        (void)mailbox.releaseRead(ticket->slot);
                    }

                    const auto now = std::chrono::steady_clock::now();
                    if (now < nextStats) continue;
                    const auto intervalMs =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - intervalStart).count();
                    MetricsSnapshot snapshot;
                    {
                        std::lock_guard<std::mutex> lock(metricsMutex);
                        snapshot = metrics.snapshot(
                            static_cast<std::uint64_t>(
                                std::max<std::int64_t>(1, intervalMs)));
                        metrics.resetInterval();
                    }
                    const std::uint64_t intervalDropped = snapshot.dropped;
                    const std::uint64_t intervalPressureDrops =
                        pressureDrops.exchange(0);
                    cumulativeDropped += intervalDropped;
                    snapshot.dropped = cumulativeDropped;

                    if (snapshot.fps_tenths
                            < config_.camera.minimum_fps * 10U
                        || intervalPressureDrops != 0) {
                        ++unhealthySeconds;
                        healthySeconds = 0;
                    }
                    else {
                        unhealthySeconds = 0;
                        ++healthySeconds;
                    }
                    if (desiredFps.load() == config_.camera.target_fps
                        && unhealthySeconds >= 2U) {
                        desiredFps.store(config_.camera.minimum_fps);
                        std::cerr << "warning: adaptive display rate reduced to "
                                  << config_.camera.minimum_fps
                                  << " FPS to preserve latency\n";
                    }
                    else if (desiredFps.load() != config_.camera.target_fps
                             && healthySeconds >= 10U) {
                        desiredFps.store(config_.camera.target_fps);
                        std::cout << "adaptive display rate restored to "
                                  << config_.camera.target_fps << " FPS"
                                  << std::endl;
                    }
                    snapshot.adaptive_degraded =
                        desiredFps.load() != config_.camera.target_fps;
                    const int recordLength = std::snprintf(
                        operationalRecord.data(), operationalRecord.size(),
                        "fps=%u.%u latency_p95_ms=%u dropped_total=%llu "
                        "state=%s\n",
                        snapshot.fps_tenths / 10U,
                        snapshot.fps_tenths % 10U,
                        snapshot.latency_p95_ms,
                        static_cast<unsigned long long>(snapshot.dropped),
                        snapshot.adaptive_degraded ? "degraded" : "healthy");
                    if (recordLength > 0
                        && static_cast<std::size_t>(recordLength)
                            < operationalRecord.size()) {
                        writeOperationalRecord(
                            STDOUT_FILENO, operationalRecord.data(),
                            static_cast<std::size_t>(recordLength));
                    }
                    if (config_.render.osd_enabled && now >= nextOsd
                        && overlay.render(snapshot, state.load(),
                                          config_.camera.target_fps)) {
                        const IoStatus osdStatus = display.panel.writeRectangle(
                            {0, 19, StatusOverlay::kWidth, StatusOverlay::kHeight},
                            overlay.pixels(), overlay.pixelCount());
                        IoStatus finalOsdStatus = osdStatus;
                        if (finalOsdStatus != IoStatus::Ok
                            && !panelResetAttempted) {
                            panelResetAttempted = true;
                            {
                                std::lock_guard<std::mutex> lock(metricsMutex);
                                metrics.onSpiError();
                            }
                            finalOsdStatus = display.panel.resetAndInitialize();
                            if (finalOsdStatus == IoStatus::Ok) {
                                finalOsdStatus = display.panel.writeRectangle(
                                    {0, 19, StatusOverlay::kWidth,
                                     StatusOverlay::kHeight},
                                    overlay.pixels(), overlay.pixelCount());
                            }
                        }
                        if (finalOsdStatus != IoStatus::Ok) {
                            fault.store(CycleFault::Spi);
                            mailbox.stop();
                        }
                    }
                    if (now >= nextOsd) {
                        nextOsd = now
                            + std::chrono::milliseconds{
                                config_.render.osd_period_ms};
                    }
                    intervalStart = now;
                    nextStats = now
                        + std::chrono::milliseconds{
                            config_.runtime.stats_period_ms};
                }
            });

            while (!stop_requested_.load()
                   && fault.load() == CycleFault::None) {
                std::this_thread::sleep_for(20ms);
            }
            mailbox.stop();
            capture.join();
            presenter.join();
            media.stop();

            if (stop_requested_.load()) break;
            if (fault.load() == CycleFault::Spi) return ExitCode::SpiFailed;
            lastFailure = fault.load() == CycleFault::Rga
                ? ExitCode::RgaFailed : ExitCode::CameraFailed;
            std::cerr << "Pipeline fault: "
                      << (fault.load() == CycleFault::Rga ? "RGA" : "camera")
                      << '\n';
        }

        if (!recovery.beginRecovery()) {
            (void)showLifecycle(PipelineState::Failed);
            return lastFailure;
        }
        if (showLifecycle(PipelineState::Recovering) != IoStatus::Ok) {
            return ExitCode::SpiFailed;
        }
        std::cerr << "Recovering media pipeline, attempt "
                  << recovery.attempts() << '/'
                  << config_.runtime.maximum_restarts << ", backoff "
                  << recovery.backoff().count() << " ms\n";
        const auto deadline = std::chrono::steady_clock::now() + recovery.backoff();
        while (!stop_requested_.load()
               && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(20ms);
        }
    }
    recovery.markStopping();
    return ExitCode::Success;
}

} // namespace camera_display
