#include "lvgl_st7789_display.hpp"

#include <algorithm>

namespace display {

LvglSt7789Display::LvglSt7789Display(hardware::St7789& panel, uint16_t buffer_lines)
    : panel_(panel), buffer_lines_(buffer_lines)
{
}

bool LvglSt7789Display::init()
{
    const auto& config = panel_.config();
    if (buffer_lines_ == 0) return false;
    const size_t buffer_bytes = static_cast<size_t>(config.width) * buffer_lines_ * 2U;
    draw_buffer_.resize(buffer_bytes);
    transmit_buffer_.resize(buffer_bytes);

    display_ = lv_display_create(config.width, config.height);
    if (display_ == nullptr) return false;
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, flushCallback);
    lv_display_set_buffers(display_, draw_buffer_.data(), nullptr,
                           static_cast<uint32_t>(draw_buffer_.size()),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return true;
}

void LvglSt7789Display::flushCallback(lv_display_t* display,
                                      const lv_area_t* area,
                                      uint8_t* pixels)
{
    auto* self = static_cast<LvglSt7789Display*>(lv_display_get_user_data(display));
    self->flush(area, pixels);
    lv_display_flush_ready(display);
}

void LvglSt7789Display::flush(const lv_area_t* area, const uint8_t* pixels)
{
    const int32_t width = area->x2 - area->x1 + 1;
    const int32_t height = area->y2 - area->y1 + 1;
    if (width <= 0 || height <= 0) return;

    const size_t byte_count = static_cast<size_t>(width) * static_cast<size_t>(height) * 2U;
    if (transmit_buffer_.size() < byte_count) transmit_buffer_.resize(byte_count);
    for (size_t i = 0; i < byte_count; i += 2) {
        transmit_buffer_[i] = pixels[i + 1];
        transmit_buffer_[i + 1] = pixels[i];
    }

    last_status_ = panel_.setAddressWindow(static_cast<uint16_t>(area->x1),
                                           static_cast<uint16_t>(area->y1),
                                           static_cast<uint16_t>(area->x2),
                                           static_cast<uint16_t>(area->y2));
    if (last_status_ == bsp::Status::ok) {
        last_status_ = panel_.writePixelBytes(transmit_buffer_.data(), byte_count);
    }
}

} // namespace display

