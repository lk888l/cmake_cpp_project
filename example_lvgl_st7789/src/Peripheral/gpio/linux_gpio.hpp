#pragma once

#include "gpio/bsp_gpio.hpp"

#include <string>

namespace bsp {

class LinuxGpioOutput final : public OutputPin {
public:
    LinuxGpioOutput(std::string chip_path, unsigned int line_offset, std::string consumer);
    ~LinuxGpioOutput() override;

    Status init(bool initial_high) override;
    Status set(bool high) override;
    void deinit() override;
    bool isInitialized() const override { return line_fd_ >= 0; }

private:
    std::string chip_path_;
    unsigned int line_offset_;
    std::string consumer_;
    int line_fd_ = -1;
};

} // namespace bsp

