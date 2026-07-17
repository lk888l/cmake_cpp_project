#pragma once

#include "bsp/spi/spi_bus.hpp"

#include <string>

namespace bsp {

struct LinuxSpiConfig {
    std::string device{"/dev/spidev0.0"};
    std::uint32_t speed_hz{40'000'000};
    std::uint8_t mode{0};
    std::uint8_t bits_per_word{8};
    std::size_t max_transfer_bytes{4096};
};

class LinuxSpiBus final : public SpiBus {
public:
    explicit LinuxSpiBus(LinuxSpiConfig config);
    ~LinuxSpiBus() override;

    Status init() override;
    Status write(const std::uint8_t* data, std::size_t length) override;
    void deinit() override;
    [[nodiscard]] bool isInitialized() const override { return fd_ >= 0; }

private:
    LinuxSpiConfig config_;
    int fd_{-1};
};

} // namespace bsp
