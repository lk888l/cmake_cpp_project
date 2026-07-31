#include "app/application.hpp"

#include "core/config.hpp"
#include "platform/rockchip/preflight.hpp"
#include "platform/rockchip/probe.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::atomic_bool stopRequested{};
static_assert(ATOMIC_BOOL_LOCK_FREE == 2,
              "the signal stop flag must always be lock-free");

void signalHandler(int) noexcept
{
    stopRequested.store(true, std::memory_order_relaxed);
}

struct Options final {
    std::string config_path{"/userdata/camera-display/camera-display.ini"};
    bool config_explicit{};
    bool probe{};
    bool self_test_lcd{};
    bool no_osd{};
    bool help{};
    bool version{};
    std::optional<std::uint32_t> fps;
    std::optional<std::uint32_t> spi_hz;
};

template <typename T>
bool parseUnsigned(std::string_view text, T& value)
{
    T result{};
    const auto parsed =
        std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        return false;
    }
    value = result;
    return true;
}

void usage(const char* executable)
{
    std::cout
        << "Usage: " << executable << " [options]\n"
        << "  --config PATH       strict INI configuration\n"
        << "  --probe             report hardware/media readiness without claiming devices\n"
        << "  --self-test lcd     show LCD color/order/orientation/throughput pattern\n"
        << "  --fps N             override target frame rate (1..30)\n"
        << "  --spi-hz N          override SPI clock\n"
        << "  --no-osd            disable top status overlay\n"
        << "  --version           print version\n"
        << "  --help              show this help\n";
}

bool parseOptions(int argc, char** argv, Options& options)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--config" && index + 1 < argc) {
            options.config_path = argv[++index];
            options.config_explicit = true;
        }
        else if (argument == "--probe") options.probe = true;
        else if (argument == "--no-osd") options.no_osd = true;
        else if (argument == "--help" || argument == "-h") options.help = true;
        else if (argument == "--version") options.version = true;
        else if (argument == "--self-test" && index + 1 < argc
                 && std::string_view{argv[index + 1]} == "lcd") {
            ++index;
            options.self_test_lcd = true;
        }
        else if (argument == "--fps" && index + 1 < argc) {
            std::uint32_t value{};
            if (!parseUnsigned(std::string_view{argv[++index]}, value)) return false;
            options.fps = value;
        }
        else if (argument == "--spi-hz" && index + 1 < argc) {
            std::uint32_t value{};
            if (!parseUnsigned(std::string_view{argv[++index]}, value)) return false;
            options.spi_hz = value;
        }
        else {
            std::cerr << "Unknown or incomplete option: " << argument << '\n';
            return false;
        }
    }
    const unsigned int modes =
        static_cast<unsigned int>(options.probe)
        + static_cast<unsigned int>(options.self_test_lcd);
    return modes <= 1U;
}

bool exists(const std::string& path)
{
    std::ifstream input(path);
    return input.good();
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) {
        usage(argv[0]);
        return static_cast<int>(camera_display::ExitCode::InvalidArguments);
    }
    if (options.help) {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }
    if (options.version) {
        std::cout << CAMERA_DISPLAY_VERSION << '\n';
        return EXIT_SUCCESS;
    }
    if (options.probe) {
        return static_cast<int>(camera_display::runProbe(std::cout));
    }

    camera_display::AppConfig config;
    if (options.config_explicit || exists(options.config_path)) {
        camera_display::ConfigResult loaded =
            camera_display::loadConfigFile(options.config_path);
        if (!loaded.errors.empty()) {
            for (const auto& error : loaded.errors) {
                std::cerr << "configuration: " << error << '\n';
            }
            return static_cast<int>(camera_display::ExitCode::InvalidArguments);
        }
        config = std::move(loaded.config);
    }
    if (options.fps) {
        config.camera.target_fps = *options.fps;
        config.camera.minimum_fps =
            std::min(config.camera.minimum_fps, *options.fps);
    }
    if (options.spi_hz) config.display.spi_hz = *options.spi_hz;
    if (options.no_osd) config.render.osd_enabled = false;
    const auto errors = camera_display::validateConfig(config);
    if (!errors.empty()) {
        for (const auto& error : errors) {
            std::cerr << "configuration: " << error << '\n';
        }
        return static_cast<int>(camera_display::ExitCode::InvalidArguments);
    }
    const camera_display::PreflightResult readiness =
        camera_display::preflight(config, !options.self_test_lcd);
    if (!readiness.errors.empty()) {
        for (const auto& error : readiness.errors) {
            std::cerr << "preflight: " << error << '\n';
        }
        return static_cast<int>(
            readiness.camera_occupied
                ? camera_display::ExitCode::CameraBusy
                : camera_display::ExitCode::PreflightFailed);
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    camera_display::Application application{std::move(config), stopRequested};
    const camera_display::ExitCode exitCode =
        options.self_test_lcd ? application.selfTestLcd() : application.run();
    return static_cast<int>(exitCode);
}
