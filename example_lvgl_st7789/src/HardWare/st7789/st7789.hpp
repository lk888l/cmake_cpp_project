#pragma once

#include "gpio/bsp_gpio.hpp"
#include "spi/bsp_spi.hpp"

#include <cstddef>
#include <cstdint>

namespace hardware {

struct St7789Config {
    uint16_t width = 240;
    uint16_t height = 240;
    uint16_t x_offset = 0;
    uint16_t y_offset = 0;
    uint16_t rotation_deg = 0;
    bool bgr = true;
    bool invert_colors = true;
};

class St7789 {
public:
    St7789(bsp::SpiBus& spi,
           bsp::OutputPin& dc,
           bsp::OutputPin& reset,
           bsp::OutputPin& backlight,
           St7789Config config);

    bsp::Status init();
    bsp::Status setBacklight(bool on);
    bsp::Status setAddressWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    bsp::Status writePixelBytes(const uint8_t* bytes, size_t length);
    const St7789Config& config() const { return config_; }

private:
    bsp::Status command(uint8_t value);
    bsp::Status data(const uint8_t* bytes, size_t length);
    bsp::Status commandData(uint8_t command_value, const uint8_t* bytes, size_t length);

    bsp::SpiBus& spi_;
    bsp::OutputPin& dc_;
    bsp::OutputPin& reset_;
    bsp::OutputPin& backlight_;
    St7789Config config_;
};

} // namespace hardware

