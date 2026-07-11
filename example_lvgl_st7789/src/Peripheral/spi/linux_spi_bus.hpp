#pragma once

#include "spi/bsp_spi.hpp"

#include <string>

namespace bsp {

struct LinuxSpiConfig {
    std::string device = "/dev/spidev0.0";
    uint32_t speed_hz = 40'000'000;
    uint8_t mode = 0;
    uint8_t bits_per_word = 8;
    size_t max_transfer_bytes = 4096;
};

class LinuxSpiBus final : public SpiBus {
public:
    explicit LinuxSpiBus(LinuxSpiConfig config);
    ~LinuxSpiBus() override;

    Status init() override;
    Status write(const uint8_t* data, size_t length) override;
    void deinit() override;
    bool isInitialized() const override { return fd_ >= 0; }

private:
    LinuxSpiConfig config_;
    int fd_ = -1;
};

} // namespace bsp

