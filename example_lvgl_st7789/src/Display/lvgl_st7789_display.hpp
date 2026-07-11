#pragma once

#include "lvgl.h"
#include "st7789.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace display {

class LvglSt7789Display {
public:
    explicit LvglSt7789Display(hardware::St7789& panel, uint16_t buffer_lines = 24);
    bool init();
    lv_display_t* handle() const { return display_; }
    bsp::Status lastStatus() const { return last_status_; }

private:
    static void flushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);
    void flush(const lv_area_t* area, const uint8_t* pixels);

    hardware::St7789& panel_;
    uint16_t buffer_lines_;
    lv_display_t* display_ = nullptr;
    std::vector<uint8_t> draw_buffer_;
    std::vector<uint8_t> transmit_buffer_;
    bsp::Status last_status_ = bsp::Status::ok;
};

} // namespace display

