#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "gyro_reader_app.hpp"
#include "icm42688/icm42688.hpp"
#include "linux_i2c_bus.hpp"

namespace {

constexpr const char* kDefaultBus = "/dev/i2c-3";
constexpr uint8_t kDefaultAddress = 0x69;
constexpr uint32_t kDefaultClockHz = 400000;
constexpr uint32_t kDefaultPeriodMs = 10;

std::atomic_bool g_running{true};

struct Options {
    std::string bus = kDefaultBus;
    uint8_t address = kDefaultAddress;
    uint32_t period_ms = kDefaultPeriodMs;
    bool help = false;
};

void printUsage(const char* program)
{
    std::cout << "Usage: " << program << " [--bus /dev/i2c-3] [--addr 0x69] [--period-ms 10]\n"
              << "\n"
              << "Defaults:\n"
              << "  --bus       /dev/i2c-3\n"
              << "  --addr      0x69\n"
              << "  --period-ms 10\n";
}

bool parseUnsigned(const std::string& text, unsigned long max_value, unsigned long& value)
{
    char* end = nullptr;
    const int base = text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0 ? 16 : 10;
    value = std::strtoul(text.c_str(), &end, base);
    return end != text.c_str() && end != nullptr && *end == '\0' && value <= max_value;
}

bool parseArgs(int argc, char* argv[], Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.help = true;
            return true;
        }
        if (arg == "--bus") {
            if (++i >= argc) {
                std::cerr << "--bus requires a value\n";
                return false;
            }
            options.bus = argv[i];
            continue;
        }
        if (arg == "--addr") {
            if (++i >= argc) {
                std::cerr << "--addr requires a value\n";
                return false;
            }
            unsigned long value = 0;
            if (!parseUnsigned(argv[i], 0x7F, value)) {
                std::cerr << "--addr must be a 7-bit I2C address, e.g. 0x69\n";
                return false;
            }
            options.address = static_cast<uint8_t>(value);
            continue;
        }
        if (arg == "--period-ms") {
            if (++i >= argc) {
                std::cerr << "--period-ms requires a value\n";
                return false;
            }
            unsigned long value = 0;
            if (!parseUnsigned(argv[i], 60000, value) || value == 0) {
                std::cerr << "--period-ms must be in range 1..60000\n";
                return false;
            }
            options.period_ms = static_cast<uint32_t>(value);
            continue;
        }

        std::cerr << "unknown argument: " << arg << "\n";
        return false;
    }
    return true;
}

void delayMs(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void handleSignal(int)
{
    g_running.store(false);
}

} // namespace

int main(int argc, char* argv[])
{
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    Options options;
    if (!parseArgs(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }
    if (options.help) {
        printUsage(argv[0]);
        return 0;
    }

    try {
        bsp::LinuxI2CBus bus({options.bus});
        const bsp::I2CStatus bus_status = bus.init();
        if (bus_status != bsp::I2CStatus::ok) {
            std::cerr << "failed to init I2C bus: " << bsp::toString(bus_status) << "\n";
            return 1;
        }

        auto device_result = bus.createDevice(options.address, kDefaultClockHz);
        if (!device_result) {
            std::cerr << "failed to create ICM42688 I2C device on " << options.bus
                      << " addr=0x" << std::hex << static_cast<int>(options.address)
                      << ": " << bsp::toString(device_result.status) << std::dec << "\n";
            return 1;
        }

        hardware::Icm42688 imu(*device_result.device, hardware::Icm42688Config{delayMs});
        const hardware::Icm42688Status imu_status = imu.initialize();
        if (imu_status != hardware::Icm42688Status::ok) {
            std::cerr << "failed to initialize ICM42688: "
                      << hardware::toString(imu_status) << "\n";
            return 1;
        }

        uint8_t who_am_i = 0;
        if (imu.readWhoAmI(who_am_i) == hardware::Icm42688Status::ok) {
            std::cerr << "ICM42688 detected, WHO_AM_I=0x"
                      << std::hex << static_cast<int>(who_am_i) << std::dec << "\n";
        }

        app::GyroReaderApp reader(imu, app::GyroReaderConfig{options.period_ms, true});
        reader.run(g_running);
    } catch (const std::exception& ex) {
        std::cerr << "fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
