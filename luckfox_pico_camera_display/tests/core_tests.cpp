#include "app/application.hpp"
#include "core/config.hpp"
#include "core/frame_mailbox.hpp"
#include "core/layout.hpp"
#include "core/metrics.hpp"
#include "core/recovery_policy.hpp"
#include "core/rgb565.hpp"
#include "core/status_overlay.hpp"
#include "drivers/st7789.hpp"

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures{};

void check(bool condition, const std::string& message)
{
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

void testConfiguration()
{
    camera_display::AppConfig config;
    check(camera_display::validateConfig(config).empty(), "default configuration is valid");
    config.camera.buffer_count = 3;
    check(!camera_display::validateConfig(config).empty(), "three camera buffers rejected");
    config.camera.buffer_count = 2;
    config.display.backlight_line = config.display.dc_line;
    check(!camera_display::validateConfig(config).empty(), "duplicate GPIO rejected");

    const std::string path{"camera-display-test.ini"};
    {
        std::ofstream output(path);
        output << "[camera]\n"
                  "target_fps=25\n"
                  "[display]\n"
                  "spi_hz=20000000\n"
                  "[render]\n"
                  "osd_enabled=false\n";
    }
    const auto loaded = camera_display::loadConfigFile(path);
    (void)std::remove(path.c_str());
    check(loaded.errors.empty(), "strict valid INI accepted");
    check(loaded.config.camera.target_fps == 25, "INI camera override");
    check(loaded.config.display.spi_hz == 20'000'000, "INI SPI override");
    check(!loaded.config.render.osd_enabled, "INI boolean override");

    const std::string invalidPath{"camera-display-invalid-test.ini"};
    {
        std::ofstream output(invalidPath);
        output << "[camera]\n"
                  "target_fps=thirty\n"
                  "unknown_option=1\n";
    }
    const auto invalid = camera_display::loadConfigFile(invalidPath);
    (void)std::remove(invalidPath.c_str());
    check(invalid.errors.size() >= 2,
          "invalid values and unknown INI keys are both rejected");
}

void testLayout()
{
    const auto letterbox = camera_display::calculateLayout(
        640, 360, 240, 240, camera_display::AspectMode::Letterbox);
    check(letterbox.source_crop.width == 640 && letterbox.source_crop.height == 360,
          "letterbox keeps complete source");
    check(letterbox.destination.x == 0 && letterbox.destination.y == 52,
          "letterbox is vertically centered");
    check(letterbox.destination.width == 240 && letterbox.destination.height == 135,
          "letterbox renders 240x135");

    const auto crop = camera_display::calculateLayout(
        640, 360, 240, 240, camera_display::AspectMode::CenterCrop);
    check(crop.source_crop.x == 140 && crop.source_crop.width == 360,
          "center crop uses central square");
    check(crop.destination.width == 240 && crop.destination.height == 240,
          "center crop fills display");
}

void testRgb565()
{
    const std::uint16_t pixels[] = {0xF800, 0x07E0, 0x001F};
    std::uint8_t encoded[6]{};
    camera_display::encodeRgb565BigEndian(pixels, encoded, 3);
    check(encoded[0] == 0xF8 && encoded[1] == 0x00, "red byte order");
    check(encoded[2] == 0x07 && encoded[3] == 0xE0, "green byte order");
    check(encoded[4] == 0x00 && encoded[5] == 0x1F, "blue byte order");
}

void testMailbox()
{
    using Mailbox = camera_display::LatestFrameMailbox<2>;
    Mailbox mailbox;
    const auto first = mailbox.acquireForWrite();
    check(first.has_value(), "first producer slot available");
    check(mailbox.publish(*first, {1, 100, {}}), "first frame published");

    const auto replacement = mailbox.acquireForWrite();
    check(replacement == first, "ready frame is reused for latest-frame replacement");
    check(mailbox.publish(*replacement, {2, 200, {}}), "replacement published");
    check(mailbox.overwrittenCount() == 1, "overwrite counted");

    const auto ticket = mailbox.waitForRead(std::chrono::milliseconds{1});
    check(ticket.has_value() && ticket->metadata.sequence == 2,
          "consumer receives newest frame");
    check(mailbox.releaseRead(ticket->slot), "consumer slot released");
    mailbox.stop();
    check(!mailbox.acquireForWrite().has_value(), "stopped mailbox rejects producer");

    Mailbox blockedMailbox;
    bool waiterReleased{};
    std::thread waiter([&] {
        waiterReleased =
            !blockedMailbox.waitForRead(std::chrono::seconds{5}).has_value();
    });
    blockedMailbox.stop();
    waiter.join();
    check(waiterReleased, "stop immediately releases waiting consumer");
}

class FakeSpi final : public camera_display::SpiBus {
public:
    camera_display::IoStatus open() override
    {
        opened = true;
        return camera_display::IoStatus::Ok;
    }
    camera_display::IoStatus write(const std::uint8_t* bytes,
                                   std::size_t size) override
    {
        if (!opened) return camera_display::IoStatus::InvalidState;
        written.insert(written.end(), bytes, bytes + size);
        statistics_.bytes += size;
        ++statistics_.transactions;
        return camera_display::IoStatus::Ok;
    }
    void close() noexcept override { opened = false; }
    bool isOpen() const noexcept override { return opened; }
    camera_display::SpiStatistics statistics() const noexcept override
    {
        return statistics_;
    }
    bool opened{true};
    std::vector<std::uint8_t> written;
    camera_display::SpiStatistics statistics_;
};

class FakePin final : public camera_display::OutputPin {
public:
    camera_display::IoStatus request(bool initialHigh) override
    {
        requested = true;
        value = initialHigh;
        return camera_display::IoStatus::Ok;
    }
    camera_display::IoStatus set(bool high) override
    {
        if (!requested) return camera_display::IoStatus::InvalidState;
        value = high;
        return camera_display::IoStatus::Ok;
    }
    void release() noexcept override { requested = false; }
    bool isRequested() const noexcept override { return requested; }
    bool requested{true};
    bool value{};
};

void testDisplayValidation()
{
    FakeSpi spi;
    FakePin dc;
    FakePin reset;
    FakePin backlight;
    camera_display::DisplayConfig config;
    config.width = 0;
    camera_display::St7789 invalid{spi, dc, reset, backlight, config};
    check(invalid.initialize() == camera_display::IoStatus::InvalidArgument,
          "invalid panel dimensions rejected before transfer");
    check(spi.written.empty(), "invalid panel configuration performs no SPI I/O");
}

void testMetrics()
{
    camera_display::RuntimeMetrics metrics;
    for (std::uint32_t index = 0; index < 30; ++index) {
        metrics.onCaptured();
        metrics.onConverted();
        metrics.onDisplayed(index);
    }
    metrics.onDropped(2);
    const auto snapshot = metrics.snapshot(1000);
    check(snapshot.fps_tenths == 300, "30 FPS represented in tenths");
    check(snapshot.latency_p95_ms == 28, "latency p95 calculated");
    check(snapshot.dropped == 2, "drops counted");
}

void testRecovery()
{
    camera_display::RecoveryPolicy policy{3};
    policy.markRunning();
    check(policy.beginRecovery(), "first recovery allowed");
    check(policy.backoff() == std::chrono::milliseconds{250}, "first backoff");
    policy.markRunning();
    check(policy.beginRecovery(), "second recovery allowed");
    check(policy.backoff() == std::chrono::milliseconds{1000}, "second backoff");
    policy.markRunning();
    check(policy.beginRecovery(), "third recovery allowed");
    policy.markRunning();
    check(!policy.beginRecovery(), "fourth recovery rejected");
    check(policy.state() == camera_display::PipelineState::Failed,
          "exhaustion enters failed state");
}

void testExitCodes()
{
    using camera_display::ExitCode;
    check(static_cast<int>(ExitCode::Success) == 0, "success exit code is zero");
    check(static_cast<int>(ExitCode::InvalidArguments) == 2,
          "invalid argument exit code is stable");
    check(static_cast<int>(ExitCode::PreflightFailed) == 3,
          "preflight exit code is stable");
    check(static_cast<int>(ExitCode::CameraBusy) == 4,
          "camera-busy exit code is distinguishable");
    check(static_cast<int>(ExitCode::CameraFailed) == 10,
          "camera exit code is distinguishable");
    check(static_cast<int>(ExitCode::RgaFailed) == 11,
          "RGA exit code is distinguishable");
    check(static_cast<int>(ExitCode::SpiFailed) == 12,
          "SPI exit code is distinguishable");
    check(static_cast<int>(ExitCode::GpioFailed) == 13,
          "GPIO exit code is distinguishable");
}

void testOverlay()
{
    camera_display::StatusOverlay overlay;
    camera_display::MetricsSnapshot metrics;
    metrics.fps_tenths = 300;
    metrics.latency_p95_ms = 18;
    check(overlay.render(metrics, camera_display::PipelineState::Running, 30),
          "first overlay render is dirty");
    check(!overlay.render(metrics, camera_display::PipelineState::Running, 30),
          "unchanged overlay is clean");
    check(overlay.pixelCount()
              == static_cast<std::size_t>(camera_display::StatusOverlay::kWidth)
                   * camera_display::StatusOverlay::kHeight,
          "overlay buffer size");
}

} // namespace

int main()
{
    testConfiguration();
    testLayout();
    testRgb565();
    testMailbox();
    testDisplayValidation();
    testMetrics();
    testRecovery();
    testExitCodes();
    testOverlay();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "all camera display core tests passed\n";
    return EXIT_SUCCESS;
}
