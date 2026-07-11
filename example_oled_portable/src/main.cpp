#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

#include "app/oled_demo_app.hpp"
#include "bsp/i2c/linux_i2c_bus.hpp"
#include "display/oled_display.hpp"

namespace {

constexpr std::uint32_t kI2CClockHz = 400000;
std::atomic_bool g_running{true};

struct Options {
    std::string bus = "/dev/i2c-3";
    std::uint8_t address = 0x3C;
    std::uint32_t period_ms = 1000;
    display::Rotation rotation = display::Rotation::deg0;
    display::Controller controller = display::Controller::ssd1306;
    app::DemoMode demo = app::DemoMode::dashboard;
    bool once = false;
    bool help = false;
};

void printUsage(const char* program)
{
    std::cout << "Usage: " << program << " [options]\n\n"
              << "  --bus PATH              I2C device (default /dev/i2c-3)\n"
              << "  --addr ADDRESS          7-bit address (default 0x3C)\n"
              << "  --period-ms N           refresh period (default 1000)\n"
              << "  --rotation 0|90|180|270 display rotation (default 0)\n"
              << "  --controller ssd1306|ssd1315 (default ssd1306)\n"
              << "  --demo dashboard|graphics|grayscale\n"
              << "  --once                  render one frame and exit\n"
              << "  -h, --help              show this help\n";
}

bool parseUnsigned(const std::string& text, unsigned long maximum, unsigned long& value)
{
    char* end = nullptr;
    const int base = text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0 ? 16 : 10;
    value = std::strtoul(text.c_str(), &end, base);
    return end != text.c_str() && end != nullptr && *end == '\0' && value <= maximum;
}

bool requireValue(int argc, char* argv[], int& index, const char* option, std::string& value)
{
    if (++index >= argc) {
        std::cerr << option << " requires a value\n";
        return false;
    }
    value = argv[index];
    return true;
}

bool parseArgs(int argc, char* argv[], Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") { options.help = true; return true; }
        if (arg == "--once") { options.once = true; continue; }
        std::string value;
        if (arg == "--bus") {
            if (!requireValue(argc, argv, i, "--bus", value)) { return false; }
            options.bus = value;
        } else if (arg == "--addr") {
            if (!requireValue(argc, argv, i, "--addr", value)) { return false; }
            unsigned long parsed = 0;
            if (!parseUnsigned(value, 0x7F, parsed)) { std::cerr << "invalid I2C address\n"; return false; }
            options.address = static_cast<std::uint8_t>(parsed);
        } else if (arg == "--period-ms") {
            if (!requireValue(argc, argv, i, "--period-ms", value)) { return false; }
            unsigned long parsed = 0;
            if (!parseUnsigned(value, 60000, parsed) || parsed == 0) {
                std::cerr << "period must be 1..60000 ms\n"; return false;
            }
            options.period_ms = static_cast<std::uint32_t>(parsed);
        } else if (arg == "--rotation") {
            if (!requireValue(argc, argv, i, "--rotation", value)) { return false; }
            if (value == "0") options.rotation = display::Rotation::deg0;
            else if (value == "90") options.rotation = display::Rotation::deg90;
            else if (value == "180") options.rotation = display::Rotation::deg180;
            else if (value == "270") options.rotation = display::Rotation::deg270;
            else { std::cerr << "rotation must be 0, 90, 180 or 270\n"; return false; }
        } else if (arg == "--controller") {
            if (!requireValue(argc, argv, i, "--controller", value)) { return false; }
            if (value == "ssd1306") options.controller = display::Controller::ssd1306;
            else if (value == "ssd1315") options.controller = display::Controller::ssd1315;
            else { std::cerr << "controller must be ssd1306 or ssd1315\n"; return false; }
        } else if (arg == "--demo") {
            if (!requireValue(argc, argv, i, "--demo", value)) { return false; }
            if (value == "dashboard") options.demo = app::DemoMode::dashboard;
            else if (value == "graphics") options.demo = app::DemoMode::graphics;
            else if (value == "grayscale") options.demo = app::DemoMode::grayscale;
            else { std::cerr << "unknown demo: " << value << "\n"; return false; }
        } else {
            std::cerr << "unknown argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}

void handleSignal(int) { g_running.store(false); }
void delayMs(std::uint32_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

} // namespace

int main(int argc, char* argv[])
{
    Options options;
    if (!parseArgs(argc, argv, options)) { printUsage(argv[0]); return 2; }
    if (options.help) { printUsage(argv[0]); return 0; }
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    bsp::LinuxI2CBus bus({options.bus});
    if (bus.init() != bsp::I2CStatus::ok) {
        std::cerr << "failed to initialize I2C bus\n";
        return 1;
    }
    auto result = bus.createDevice(options.address, kI2CClockHz);
    if (!result) {
        std::cerr << "failed to open " << options.bus << " addr=0x" << std::hex
                  << static_cast<unsigned>(options.address) << ": "
                  << bsp::toString(result.status) << std::dec << "\n";
        return 1;
    }

    display::OledDisplay oled(
        *result.device, {options.rotation, options.controller, 0xCF, delayMs});
    const auto init_status = oled.initialize();
    if (init_status != display::DisplayStatus::ok) {
        std::cerr << "OLED initialization failed: " << display::toString(init_status) << "\n";
        return 1;
    }
    std::cout << "OLED ready: " << oled.width() << 'x' << oled.height()
              << ", framebuffer=" << oled.framebufferSize() << " bytes\n";

    app::OledDemoApp demo(oled, {options.demo, options.period_ms, options.once});
    const auto status = demo.run(g_running);
    oled.setPowerSave(true);
    if (status != display::DisplayStatus::ok) {
        std::cerr << "display update failed: " << display::toString(status) << "\n";
        return 1;
    }
    return 0;
}
