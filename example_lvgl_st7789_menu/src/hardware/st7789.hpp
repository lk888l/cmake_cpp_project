#pragma once

#include "bsp/gpio/output_pin.hpp"
#include "bsp/spi/spi_bus.hpp"

#include <cstddef>
#include <cstdint>

namespace hardware {

struct St7789Config {
    std::uint16_t width{240};
    std::uint16_t height{240};
    std::uint16_t x_offset{0};
    std::uint16_t y_offset{0};
    std::uint16_t rotation_deg{0};
    bool bgr{true};
    bool invert_colors{true};
};

class St7789 {
public:
    St7789(bsp::SpiBus& spi,
           bsp::OutputPin& dc,
           bsp::OutputPin& reset,
           bsp::OutputPin& backlight,
           St7789Config config);
    ~St7789();

    bsp::Status init();
    bsp::Status setBacklight(bool on);
    bsp::Status setAddressWindow(std::uint16_t x1, std::uint16_t y1,
                                 std::uint16_t x2, std::uint16_t y2);
    bsp::Status writePixelBytes(const std::uint8_t* bytes, std::size_t length);
    [[nodiscard]] const St7789Config& config() const { return config_; }

private:
    bsp::Status command(std::uint8_t value);
    bsp::Status data(const std::uint8_t* bytes, std::size_t length);
    bsp::Status commandData(std::uint8_t command_value,
                            const std::uint8_t* bytes,
                            std::size_t length);

    bsp::SpiBus& spi_;
    bsp::OutputPin& dc_;
    bsp::OutputPin& reset_;
    bsp::OutputPin& backlight_;
    St7789Config config_;
};

} // namespace hardware
