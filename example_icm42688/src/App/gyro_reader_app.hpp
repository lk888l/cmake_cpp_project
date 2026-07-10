#pragma once

#include <atomic>
#include <cstdint>

#include "icm42688/icm42688.hpp"

namespace app {

struct GyroReaderConfig {
    uint32_t period_ms = 10;
    bool print_header = true;
};

class GyroReaderApp {
public:
    GyroReaderApp(hardware::Icm42688& imu, GyroReaderConfig config);

    void run(const std::atomic_bool& running);

private:
    hardware::Icm42688& imu_;
    GyroReaderConfig config_;
};

} // namespace app
