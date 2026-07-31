#pragma once

#include "core/config.hpp"

#include <atomic>

namespace camera_display {

enum class ExitCode : int {
    Success = 0,
    InvalidArguments = 2,
    PreflightFailed = 3,
    CameraBusy = 4,
    CameraFailed = 10,
    RgaFailed = 11,
    SpiFailed = 12,
    GpioFailed = 13,
    RuntimeFailed = 14,
};

class Application final {
public:
    Application(AppConfig config, std::atomic_bool& stopRequested);

    ExitCode run();
    ExitCode selfTestLcd();

private:
    AppConfig config_;
    std::atomic_bool& stop_requested_;
};

} // namespace camera_display
