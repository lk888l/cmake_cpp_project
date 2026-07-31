#pragma once

#include "core/io.hpp"

#include <string>

namespace camera_display {

class LinuxSpiBus final : public SpiBus {
public:
    LinuxSpiBus(std::string device, std::uint32_t speedHz,
                std::size_t maximumTransferBytes);
    ~LinuxSpiBus() override;

    IoStatus open() override;
    IoStatus write(const std::uint8_t* bytes, std::size_t size) override;
    void close() noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override { return fd_ >= 0; }
    [[nodiscard]] SpiStatistics statistics() const noexcept override { return statistics_; }

private:
    std::string device_;
    std::uint32_t speed_hz_;
    std::size_t maximum_transfer_bytes_;
    int fd_{-1};
    SpiStatistics statistics_;
};

} // namespace camera_display
