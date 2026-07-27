#include "app/menu_application.hpp"
#include "bsp/bsp_status.hpp"
#include "bsp/gpio/linux_gpio_output.hpp"
#include "bsp/spi/linux_spi_bus.hpp"
#include "display/lvgl_st7789_display.hpp"
#include "hardware/st7789.hpp"
#include "input/button.hpp"
#include "input/three_key_lvgl_input.hpp"
#include "runtime/lvgl_memory_pool.hpp"
#include "runtime/render_policy.hpp"

#include "lvgl.h"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <signal.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t stop_requested = 0;
volatile std::sig_atomic_t shutdown_timeout_s = 3;

void forceExitSignal(int)
{
    constexpr char message[] = "forced exit: shutdown deadline expired\n";
    (void)::write(STDERR_FILENO, message, sizeof(message) - 1U);
    ::_exit(124);
}

void stopSignal(int signalNumber)
{
    if (stop_requested != 0) {
        ::_exit(128 + signalNumber);
    }
    stop_requested = 1;
    constexpr char message[] = "shutdown requested; press Ctrl+C again to force exit\n";
    (void)::write(STDERR_FILENO, message, sizeof(message) - 1U);
    (void)::alarm(static_cast<unsigned int>(shutdown_timeout_s));
}

void installSignalHandlers(std::uint32_t timeoutSeconds)
{
    shutdown_timeout_s = static_cast<std::sig_atomic_t>(timeoutSeconds);

    struct sigaction stopAction {};
    stopAction.sa_handler = stopSignal;
    ::sigemptyset(&stopAction.sa_mask);
    stopAction.sa_flags = 0;
    if (::sigaction(SIGINT, &stopAction, nullptr) < 0
        || ::sigaction(SIGTERM, &stopAction, nullptr) < 0) {
        throw std::system_error(errno, std::generic_category(), "sigaction(stop)");
    }

    struct sigaction alarmAction {};
    alarmAction.sa_handler = forceExitSignal;
    ::sigemptyset(&alarmAction.sa_mask);
    alarmAction.sa_flags = 0;
    if (::sigaction(SIGALRM, &alarmAction, nullptr) < 0) {
        throw std::system_error(errno, std::generic_category(), "sigaction(alarm)");
    }
}

struct Options {
    std::string spi_device{"/dev/spidev0.0"};
    std::string gpio_chip{"/dev/gpiochip0"};
    std::string key_gpio_chip{"/dev/gpiochip0"};
    std::uint32_t spi_hz{40'000'000};
    int dc_line{-1};
    int reset_line{-1};
    int backlight_line{-1};
    int key_up_line{-1};
    int key_down_line{-1};
    int key_ok_line{-1};
    bool keys_active_high{false};
    std::uint16_t width{240};
    std::uint16_t height{240};
    std::uint16_t x_offset{0};
    std::uint16_t y_offset{0};
    std::uint16_t rotation{0};
    runtime::RenderProfile render_profile{runtime::RenderProfile::Auto};
    std::optional<std::uint16_t> buffer_lines;
    std::optional<std::uint16_t> target_fps;
    std::optional<std::uint32_t> lvgl_extra_heap_kib;
    std::uint32_t stats_interval_ms{0};
    std::uint32_t shutdown_timeout_s{3};
    std::uint32_t debounce_ms{25};
    std::uint32_t long_press_ms{600};
    std::uint32_t double_click_ms{250};
    std::uint32_t repeat_delay_ms{400};
    std::uint32_t repeat_period_ms{100};
};

template <typename T>
T parseUnsigned(std::string_view text, std::string_view name)
{
    static_assert(std::is_unsigned_v<T>);
    if (text.empty() || text.front() == '-') {
        throw std::runtime_error("invalid " + std::string(name) + ": " + std::string(text));
    }
    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (error != std::errc{} || end != text.data() + text.size() ||
        value > std::numeric_limits<T>::max()) {
        throw std::runtime_error("invalid " + std::string(name) + ": " + std::string(text));
    }
    return static_cast<T>(value);
}

int parseLine(std::string_view text, std::string_view name)
{
    const auto value = parseUnsigned<unsigned int>(text, name);
    if (value > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(std::string(name) + " is out of range");
    }
    return static_cast<int>(value);
}

void printUsage(const char* program)
{
    std::cout
        << "Usage: " << program << " --dc <line> --key-up <line> --key-down <line> --key-ok <line> [options]\n"
        << "Display:\n"
        << "  --spi <path>              SPI device (default /dev/spidev0.0)\n"
        << "  --spi-hz <hz>             SPI clock (default 40000000)\n"
        << "  --gpiochip <path>         LCD GPIO chip (default /dev/gpiochip0)\n"
        << "  --dc <line>               Required D/C line offset\n"
        << "  --reset <line>            Optional reset line offset\n"
        << "  --backlight <line>        Optional backlight line offset\n"
        << "  --width/--height <px>     Panel size (default 240x240)\n"
        << "  --x-offset/--y-offset <n> Controller RAM offsets\n"
        << "  --rotation <deg>          0, 90, 180, or 270\n"
        << "  --render-profile <name>  auto, low, balanced, or quality (default auto)\n"
        << "  --buffer-lines <n>        Override profile partial-buffer height\n"
        << "  --target-fps <n>          Override profile refresh target (1-60)\n"
        << "  --lvgl-extra-heap-kib <n> Override additional LVGL pool (0 disables it)\n"
        << "  --stats-interval-ms <ms>  Print runtime statistics; 0 disables (default 0)\n"
        << "  --shutdown-timeout-s <s>  Forced-exit deadline after first signal (default 3)\n"
        << "Keys:\n"
        << "  --key-gpiochip <path>     Key GPIO chip (default /dev/gpiochip0)\n"
        << "  --key-up <line>           Required Up line offset\n"
        << "  --key-down <line>         Required Down line offset\n"
        << "  --key-ok <line>           Required Confirm line offset\n"
        << "  --keys-active-high        Use active-high keys with internal pull-down\n"
        << "  --debounce-ms <ms>        Debounce time (default 25)\n"
        << "  --long-press-ms <ms>      Confirm Back threshold (default 600)\n"
        << "  --double-click-ms <ms>    Gesture window (default 250)\n"
        << "  --repeat-delay-ms <ms>    Up/Down repeat delay (default 400)\n"
        << "  --repeat-period-ms <ms>   Up/Down repeat period (default 100)\n";
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        const auto next = [&]() -> std::string_view {
            if (++index >= argc) throw std::runtime_error("missing value for " + std::string(argument));
            return argv[index];
        };

        if (argument == "--help" || argument == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (argument == "--spi") options.spi_device = next();
        else if (argument == "--spi-hz") options.spi_hz = parseUnsigned<std::uint32_t>(next(), "spi-hz");
        else if (argument == "--gpiochip") options.gpio_chip = next();
        else if (argument == "--dc") options.dc_line = parseLine(next(), "dc");
        else if (argument == "--reset") options.reset_line = parseLine(next(), "reset");
        else if (argument == "--backlight") options.backlight_line = parseLine(next(), "backlight");
        else if (argument == "--width") options.width = parseUnsigned<std::uint16_t>(next(), "width");
        else if (argument == "--height") options.height = parseUnsigned<std::uint16_t>(next(), "height");
        else if (argument == "--x-offset") options.x_offset = parseUnsigned<std::uint16_t>(next(), "x-offset");
        else if (argument == "--y-offset") options.y_offset = parseUnsigned<std::uint16_t>(next(), "y-offset");
        else if (argument == "--rotation") options.rotation = parseUnsigned<std::uint16_t>(next(), "rotation");
        else if (argument == "--render-profile") {
            const auto profile = runtime::parseRenderProfile(next());
            if (!profile) throw std::runtime_error("render-profile must be auto, low, balanced, or quality");
            options.render_profile = *profile;
        }
        else if (argument == "--buffer-lines") options.buffer_lines = parseUnsigned<std::uint16_t>(next(), "buffer-lines");
        else if (argument == "--target-fps") options.target_fps = parseUnsigned<std::uint16_t>(next(), "target-fps");
        else if (argument == "--lvgl-extra-heap-kib") options.lvgl_extra_heap_kib = parseUnsigned<std::uint32_t>(next(), "lvgl-extra-heap-kib");
        else if (argument == "--stats-interval-ms") options.stats_interval_ms = parseUnsigned<std::uint32_t>(next(), "stats-interval-ms");
        else if (argument == "--shutdown-timeout-s") options.shutdown_timeout_s = parseUnsigned<std::uint32_t>(next(), "shutdown-timeout-s");
        else if (argument == "--key-gpiochip") options.key_gpio_chip = next();
        else if (argument == "--key-up") options.key_up_line = parseLine(next(), "key-up");
        else if (argument == "--key-down") options.key_down_line = parseLine(next(), "key-down");
        else if (argument == "--key-ok") options.key_ok_line = parseLine(next(), "key-ok");
        else if (argument == "--keys-active-high") options.keys_active_high = true;
        else if (argument == "--debounce-ms") options.debounce_ms = parseUnsigned<std::uint32_t>(next(), "debounce-ms");
        else if (argument == "--long-press-ms") options.long_press_ms = parseUnsigned<std::uint32_t>(next(), "long-press-ms");
        else if (argument == "--double-click-ms") options.double_click_ms = parseUnsigned<std::uint32_t>(next(), "double-click-ms");
        else if (argument == "--repeat-delay-ms") options.repeat_delay_ms = parseUnsigned<std::uint32_t>(next(), "repeat-delay-ms");
        else if (argument == "--repeat-period-ms") options.repeat_period_ms = parseUnsigned<std::uint32_t>(next(), "repeat-period-ms");
        else throw std::runtime_error("unknown option: " + std::string(argument));
    }
    return options;
}

void validateOptions(const Options& options)
{
    if (options.spi_device.empty() || options.gpio_chip.empty() || options.key_gpio_chip.empty()) {
        throw std::runtime_error("device paths must not be empty");
    }
    if (options.dc_line < 0) throw std::runtime_error("--dc is required");
    if (options.key_up_line < 0 || options.key_down_line < 0 || options.key_ok_line < 0) {
        throw std::runtime_error("--key-up, --key-down, and --key-ok are required");
    }
    if (options.spi_hz == 0 || options.width == 0 || options.height == 0) {
        throw std::runtime_error("SPI and panel sizes must be positive and valid");
    }
    if (options.buffer_lines && (*options.buffer_lines == 0 || *options.buffer_lines > options.height))
        throw std::runtime_error("buffer-lines must be in the panel height range");
    if (options.target_fps && (*options.target_fps == 0 || *options.target_fps > 60))
        throw std::runtime_error("target-fps must be between 1 and 60");
    if (options.lvgl_extra_heap_kib && *options.lvgl_extra_heap_kib > 1024U * 1024U)
        throw std::runtime_error("lvgl-extra-heap-kib is unreasonably large");
    if (options.stats_interval_ms != 0 && options.stats_interval_ms < 100)
        throw std::runtime_error("stats-interval-ms must be 0 or at least 100");
    if (options.shutdown_timeout_s == 0 || options.shutdown_timeout_s > 300)
        throw std::runtime_error("shutdown-timeout-s must be between 1 and 300");
    if (options.rotation != 0 && options.rotation != 90 &&
        options.rotation != 180 && options.rotation != 270) {
        throw std::runtime_error("rotation must be 0, 90, 180, or 270");
    }
    if (options.long_press_ms == 0 || options.double_click_ms == 0 ||
        options.repeat_delay_ms == 0 || options.repeat_period_ms == 0) {
        throw std::runtime_error("gesture and repeat durations must be positive");
    }

    struct LineUse { std::string_view chip; int line; std::string_view name; };
    std::vector<LineUse> lines = {
        {options.gpio_chip, options.dc_line, "dc"},
        {options.key_gpio_chip, options.key_up_line, "key-up"},
        {options.key_gpio_chip, options.key_down_line, "key-down"},
        {options.key_gpio_chip, options.key_ok_line, "key-ok"},
    };
    if (options.reset_line >= 0) lines.push_back({options.gpio_chip, options.reset_line, "reset"});
    if (options.backlight_line >= 0) lines.push_back({options.gpio_chip, options.backlight_line, "backlight"});
    for (std::size_t left = 0; left < lines.size(); ++left) {
        for (std::size_t right = left + 1; right < lines.size(); ++right) {
            if (lines[left].chip == lines[right].chip && lines[left].line == lines[right].line) {
                throw std::runtime_error("GPIO line conflict: " + std::string(lines[left].name) +
                                         " and " + std::string(lines[right].name));
            }
        }
    }
}

std::unique_ptr<bsp::OutputPin> makeOptionalPin(const Options& options,
                                                int line,
                                                const char* consumer)
{
    if (line < 0) return std::make_unique<bsp::NullOutputPin>();
    return std::make_unique<bsp::LinuxGpioOutput>(
        options.gpio_chip, static_cast<unsigned int>(line), consumer);
}

void requireStatus(bsp::Status status, std::string_view action)
{
    if (status != bsp::Status::ok) {
        throw std::runtime_error(std::string(action) + " failed: " + bsp::toString(status));
    }
}

std::optional<input::PhysicalButton> physicalButton(std::string_view id)
{
    if (id == "key-up") return input::PhysicalButton::Up;
    if (id == "key-down") return input::PhysicalButton::Down;
    if (id == "key-ok") return input::PhysicalButton::Confirm;
    return std::nullopt;
}

std::string_view buttonEventName(gpio_button::ButtonEventType type)
{
    using gpio_button::ButtonEventType;
    switch (type) {
    case ButtonEventType::Press: return "Press";
    case ButtonEventType::Release: return "Release";
    case ButtonEventType::Click: return "Click";
    case ButtonEventType::DoubleClick: return "DoubleClick";
    case ButtonEventType::LongPress: return "LongPress";
    }
    return "Unknown";
}

gpio_button::ButtonConfig makeButtonConfig(const Options& options,
                                            std::string id,
                                            int line)
{
    return gpio_button::ButtonConfig{
        .id = std::move(id),
        .chip_path = options.key_gpio_chip,
        .line_offset = static_cast<unsigned int>(line),
        .active_low = !options.keys_active_high,
        .debounce = std::chrono::milliseconds{options.debounce_ms},
        .long_press = std::chrono::milliseconds{options.long_press_ms},
        .double_click_window = std::chrono::milliseconds{options.double_click_ms},
    };
}

void printResources(const runtime::SystemResources& resources,
                    runtime::RenderProfile requestedProfile,
                    const runtime::RenderPolicy& policy)
{
    const auto printOptional = [](const auto& value) {
        if (value) std::cout << *value;
        else std::cout << "unknown";
    };

    std::cout << "build=" << LVGL_MENU_BUILD_TYPE
              << " base-lvgl-heap=" << LVGL_MENU_BASE_HEAP_KIB << "KiB\n"
              << "resources: cpu=";
    printOptional(resources.onlineCpuCount);
    std::cout << " total-memory=";
    printOptional(resources.totalMemoryKiB);
    std::cout << "KiB available-memory=";
    printOptional(resources.availableMemoryKiB);
    std::cout << "KiB\nrender: requested=" << runtime::toString(requestedProfile)
              << " resolved=" << runtime::toString(policy.profile)
              << " fps=" << policy.targetFps
              << " refresh=" << policy.refreshPeriodMs << "ms"
              << " buffer-lines=" << policy.bufferLines
              << " extra-lvgl-heap=" << policy.extraHeapKiB << "KiB\n"
              << std::flush;
}

void printMemoryStats(std::string_view label)
{
    lv_mem_monitor_t memory{};
    lv_mem_monitor(&memory);
    std::cout << label << ": lvgl-total=" << memory.total_size
              << "B max-used=" << memory.max_used
              << "B free=" << memory.free_size
              << "B largest-free=" << memory.free_biggest_size
              << "B fragmentation=" << static_cast<unsigned>(memory.frag_pct)
              << "%\n";
}

class LvglRuntime final {
public:
    LvglRuntime() { lv_init(); }
    ~LvglRuntime() { lv_deinit(); }
    LvglRuntime(const LvglRuntime&) = delete;
    LvglRuntime& operator=(const LvglRuntime&) = delete;
};

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parseOptions(argc, argv);
        validateOptions(options);
        installSignalHandlers(options.shutdown_timeout_s);

        const auto resources = runtime::detectSystemResources();
        const auto policy = runtime::resolveRenderPolicy(
            resources,
            {
                .profile = options.render_profile,
                .bufferLines = options.buffer_lines,
                .targetFps = options.target_fps,
                .extraHeapKiB = options.lvgl_extra_heap_kib,
            });
        if (policy.bufferLines == 0 || policy.bufferLines > options.height) {
            throw std::runtime_error("resolved buffer-lines exceeds panel height");
        }
        printResources(resources, options.render_profile, policy);
        if (options.buffer_lines && *options.buffer_lines < 24) {
            std::cerr
                << "warning: --buffer-lines below 24 amplifies ST7789 address-window, "
                   "GPIO, and SPI transaction overhead; omit the override unless memory "
                   "is critically constrained\n";
        }

        // The Luckfox Rockchip SPI driver rejects individual transfers larger
        // than 4 KiB; keep the userspace chunks within that controller limit.
        bsp::LinuxSpiBus spi({options.spi_device, options.spi_hz, 0, 8, 4096});
        bsp::LinuxGpioOutput dc(options.gpio_chip,
                                static_cast<unsigned int>(options.dc_line),
                                "st7789-dc");
        auto reset = makeOptionalPin(options, options.reset_line, "st7789-reset");
        auto backlight = makeOptionalPin(options, options.backlight_line, "st7789-backlight");

        requireStatus(spi.init(), "SPI init");
        requireStatus(dc.init(true), "D/C GPIO init");
        requireStatus(reset->init(true), "reset GPIO init");
        requireStatus(backlight->init(false), "backlight GPIO init");

        hardware::St7789 panel(
            spi, dc, *reset, *backlight,
            {options.width, options.height, options.x_offset, options.y_offset,
             options.rotation, true, true});
        requireStatus(panel.init(), "ST7789 init");

        // Storage must outlive lv_deinit(): LVGL global theme/cache objects can
        // remain in an added TLSF pool after all application widgets are gone.
        runtime::LvglMemoryPool extraPool;
        LvglRuntime lvglRuntime;
        {
            if (!extraPool.add(static_cast<std::size_t>(policy.extraHeapKiB) * 1024U)) {
                throw std::runtime_error("unable to add requested LVGL memory pool");
            }

            display::LvglSt7789Display display(
                panel, {policy.bufferLines, policy.refreshPeriodMs});
            if (!display.init()) throw std::runtime_error("LVGL display init failed");

            const auto firstFrameStartedAt = std::chrono::steady_clock::now();
            app::MenuApplication menu;
            if (!menu.create(lv_screen_active(), policy)) {
                throw std::runtime_error("menu UI creation failed");
            }
            std::cout << "render: large-object-layers="
                      << (menu.usesLargeObjectLayers() ? "enabled" : "disabled") << '\n'
                      << std::flush;
            lv_refr_now(display.handle());
            if (display.lastStatus() != bsp::Status::ok) {
                throw std::runtime_error(std::string("first LCD flush failed: ")
                                         + bsp::toString(display.lastStatus()));
            }
            const auto firstFrameMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - firstFrameStartedAt);
            std::cout << "first-frame=" << firstFrameMs.count() << "ms\n";
            printMemoryStats("first-frame");

            input::ThreeKeyLvglInput keys({
                std::chrono::milliseconds{options.repeat_delay_ms},
                std::chrono::milliseconds{options.repeat_period_ms},
            });
            keys.setActionSink(
                [&menu](input::InputAction action) { menu.handleAction(action); });
            const auto publishTelemetry = [&keys, &menu](
                                              input::PhysicalButton button,
                                              gpio_button::MonotonicTime now) {
                const auto snapshot = keys.snapshot(button, now);
                const auto held = std::chrono::duration_cast<std::chrono::milliseconds>(
                    snapshot.held_for);
                menu.onButtonTelemetry(
                    button, snapshot.pressed,
                    static_cast<std::uint32_t>(std::min<std::uint64_t>(
                        snapshot.press_count,
                        std::numeric_limits<std::uint32_t>::max())),
                    static_cast<std::uint32_t>(std::max<std::int64_t>(0, held.count())),
                    snapshot.last_event ? buttonEventName(*snapshot.last_event)
                                        : std::string_view{});
            };
            keys.setTelemetryCallback(
                [&publishTelemetry](input::PhysicalButton button,
                                    gpio_button::ButtonEventType,
                                    std::chrono::milliseconds) {
                    publishTelemetry(button, gpio_button::MonotonicTime{
                        std::chrono::duration_cast<gpio_button::MonotonicDuration>(
                            std::chrono::steady_clock::now().time_since_epoch())});
                });

            gpio_button::ButtonManager manager({
                makeButtonConfig(options, "key-up", options.key_up_line),
                makeButtonConfig(options, "key-down", options.key_down_line),
                makeButtonConfig(options, "key-ok", options.key_ok_line),
            });
            manager.setCallback([&keys](const gpio_button::ButtonEvent& event) {
                if (const auto button = physicalButton(event.id)) {
                    keys.push(*button, event);
                }
            });
            manager.start();

            auto previous = std::chrono::steady_clock::now();
            auto statsStartedAt = previous;
            auto previousDisplayStats = display.stats();
            std::uint64_t timerHandlerCalls{};
            std::uint64_t timerHandlerTotalUs{};
            std::uint64_t timerHandlerMaxUs{};
            std::uint32_t telemetryElapsedMs = 0;
            while (stop_requested == 0) {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previous);
                if (elapsed.count() > 0) {
                    const auto elapsedMs = static_cast<std::uint32_t>(
                        std::min<std::int64_t>(elapsed.count(), 1000));
                    lv_tick_inc(elapsedMs);
                    (void)keys.pump(gpio_button::MonotonicTime{
                        std::chrono::duration_cast<gpio_button::MonotonicDuration>(
                            now.time_since_epoch())});
                    menu.tick(elapsedMs);
                    telemetryElapsedMs += elapsedMs;
                    if (telemetryElapsedMs >= 50) {
                        const auto monotonicNow = gpio_button::MonotonicTime{
                            std::chrono::duration_cast<gpio_button::MonotonicDuration>(
                                now.time_since_epoch())};
                        publishTelemetry(input::PhysicalButton::Up, monotonicNow);
                        publishTelemetry(input::PhysicalButton::Down, monotonicNow);
                        publishTelemetry(input::PhysicalButton::Confirm, monotonicNow);
                        telemetryElapsedMs = 0;
                    }
                    previous = now;
                }

                const auto timerStartedAt = std::chrono::steady_clock::now();
                const std::uint32_t waitMs = lv_timer_handler();
                const auto timerUs = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - timerStartedAt);
                const auto timerElapsedUs = static_cast<std::uint64_t>(
                    std::max<std::int64_t>(0, timerUs.count()));
                ++timerHandlerCalls;
                timerHandlerTotalUs += timerElapsedUs;
                timerHandlerMaxUs = std::max(timerHandlerMaxUs, timerElapsedUs);

                if (display.lastStatus() != bsp::Status::ok) {
                    if (display.lastStatus() == bsp::Status::interrupted
                        && stop_requested != 0) {
                        break;
                    }
                    throw std::runtime_error(std::string("LCD flush failed: ")
                                             + bsp::toString(display.lastStatus()));
                }
                if (!manager.isRunning()) {
                    throw std::runtime_error("GPIO input stopped: "
                        + manager.lastError().value_or("unknown worker error"));
                }

                if (options.stats_interval_ms != 0
                    && std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - statsStartedAt).count() >= options.stats_interval_ms) {
                    const auto current = display.stats();
                    const auto flushes = current.flushCount - previousDisplayStats.flushCount;
                    const auto bytes = current.pixelBytes - previousDisplayStats.pixelBytes;
                    const auto flushUs = current.totalFlushTimeUs
                        - previousDisplayStats.totalFlushTimeUs;
                    std::cout << "stats: flushes=" << flushes
                              << " bytes=" << bytes
                              << " flush-ms=" << flushUs / 1000U
                              << " flush-max-us=" << current.maxFlushTimeUs
                              << " timer-avg-us="
                              << (timerHandlerCalls == 0 ? 0 : timerHandlerTotalUs / timerHandlerCalls)
                              << " timer-max-us=" << timerHandlerMaxUs << '\n';
                    printMemoryStats("stats");
                    previousDisplayStats = current;
                    statsStartedAt = now;
                    timerHandlerCalls = 0;
                    timerHandlerTotalUs = 0;
                    timerHandlerMaxUs = 0;
                }

                const auto sleepMs = waitMs == LV_NO_TIMER_READY
                    ? std::min<std::uint32_t>(policy.refreshPeriodMs, 20U)
                    : std::clamp(waitMs, 1U, 20U);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }

            manager.stop();
        }
        (void)::alarm(0);
        (void)panel.setBacklight(false);
        return 0;
    } catch (const std::exception& error) {
        (void)::alarm(0);
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
