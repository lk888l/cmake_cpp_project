#include "lvgl_demo_app.hpp"
#include "lvgl_st7789_display.hpp"
#include "gpio/linux_gpio.hpp"
#include "spi/linux_spi_bus.hpp"
#include "st7789.hpp"

#include "lvgl.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

struct Options {
    std::string spi_device = "/dev/spidev0.0";
    std::string gpio_chip = "/dev/gpiochip0";
    uint32_t spi_hz = 40'000'000;
    int dc_line = -1;
    int reset_line = -1;
    int backlight_line = -1;
    uint16_t width = 240;
    uint16_t height = 240;
    uint16_t x_offset = 0;
    uint16_t y_offset = 0;
    uint16_t rotation = 0;
    uint16_t buffer_lines = 24;
};

unsigned long parseNumber(const char* text, const char* name)
{
    char* end = nullptr;
    const unsigned long value = std::strtoul(text, &end, 0);
    if (text[0] == '\0' || (end != nullptr && *end != '\0')) {
        throw std::runtime_error(std::string("invalid ") + name + ": " + text);
    }
    return value;
}

void printUsage(const char* program)
{
    std::cout
        << "Usage: " << program << " --dc <line> [options]\n"
        << "  --spi <path>          SPI device (default /dev/spidev0.0)\n"
        << "  --spi-hz <hz>         SPI clock (default 40000000)\n"
        << "  --gpiochip <path>     GPIO character device (default /dev/gpiochip0)\n"
        << "  --dc <line>           Required D/C GPIO line offset\n"
        << "  --reset <line>        Optional reset GPIO line offset\n"
        << "  --backlight <line>    Optional backlight GPIO line offset\n"
        << "  --width/--height <px> Panel size (default 240x240)\n"
        << "  --x-offset/--y-offset Controller RAM offsets (default 0)\n"
        << "  --rotation <deg>      0, 90, 180, or 270\n"
        << "  --buffer-lines <n>    LVGL partial buffer height (default 24)\n";
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        auto next = [&]() -> const char* {
            if (++i >= argc) throw std::runtime_error("missing value for " + argument);
            return argv[i];
        };
        if (argument == "--help" || argument == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (argument == "--spi") options.spi_device = next();
        else if (argument == "--gpiochip") options.gpio_chip = next();
        else if (argument == "--spi-hz") options.spi_hz = parseNumber(next(), "spi-hz");
        else if (argument == "--dc") options.dc_line = static_cast<int>(parseNumber(next(), "dc"));
        else if (argument == "--reset") options.reset_line = static_cast<int>(parseNumber(next(), "reset"));
        else if (argument == "--backlight") options.backlight_line = static_cast<int>(parseNumber(next(), "backlight"));
        else if (argument == "--width") options.width = parseNumber(next(), "width");
        else if (argument == "--height") options.height = parseNumber(next(), "height");
        else if (argument == "--x-offset") options.x_offset = parseNumber(next(), "x-offset");
        else if (argument == "--y-offset") options.y_offset = parseNumber(next(), "y-offset");
        else if (argument == "--rotation") options.rotation = parseNumber(next(), "rotation");
        else if (argument == "--buffer-lines") options.buffer_lines = parseNumber(next(), "buffer-lines");
        else throw std::runtime_error("unknown option: " + argument);
    }
    if (options.dc_line < 0) throw std::runtime_error("--dc is required");
    return options;
}

std::unique_ptr<bsp::OutputPin> makeOptionalPin(const Options& options,
                                                int line,
                                                const char* consumer)
{
    if (line < 0) return std::make_unique<bsp::NullOutputPin>();
    return std::make_unique<bsp::LinuxGpioOutput>(options.gpio_chip,
                                                  static_cast<unsigned int>(line),
                                                  consumer);
}

void requireStatus(bsp::Status status, const char* action)
{
    if (status != bsp::Status::ok) {
        throw std::runtime_error(std::string(action) + " failed: " + bsp::toString(status));
    }
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const Options options = parseOptions(argc, argv);
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

        hardware::St7789 panel(spi, dc, *reset, *backlight,
                               {options.width, options.height,
                                options.x_offset, options.y_offset,
                                options.rotation, true, true});
        requireStatus(panel.init(), "ST7789 init");

        lv_init();
        display::LvglSt7789Display display(panel, options.buffer_lines);
        if (!display.init()) throw std::runtime_error("LVGL display init failed");
        app::createDemoUi();

        auto previous = std::chrono::steady_clock::now();
        while (true) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previous);
            if (elapsed.count() > 0) {
                lv_tick_inc(static_cast<uint32_t>(elapsed.count()));
                previous = now;
            }
            const uint32_t wait_ms = lv_timer_handler();
            if (display.lastStatus() != bsp::Status::ok) {
                throw std::runtime_error(std::string("LCD flush failed: ") +
                                         bsp::toString(display.lastStatus()));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(
                wait_ms == LV_NO_TIMER_READY ? 5U : std::max(1U, std::min(wait_ms, 10U))));
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
