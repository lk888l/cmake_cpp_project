#include "display/lvgl_st7789_display.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>

namespace display {

LvglSt7789Display::LvglSt7789Display(hardware::St7789& panel, LvglDisplayConfig config)
    : panel_(panel), config_(config)
{
}

LvglSt7789Display::~LvglSt7789Display()
{
    if (display_ != nullptr) {
        lv_display_delete(display_);
        display_ = nullptr;
    }
}

bool LvglSt7789Display::init()
{
    const auto& config = panel_.config();
    if (config_.bufferLines == 0 || config_.bufferLines > config.height
        || config_.refreshPeriodMs == 0) return false;
    const std::size_t buffer_bytes =
        static_cast<std::size_t>(config.width) * config_.bufferLines * 2U;
    draw_buffer_.resize(buffer_bytes);

    display_ = lv_display_create(config.width, config.height);
    if (display_ == nullptr) return false;
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, flushCallback);
    lv_display_set_buffers(display_, draw_buffer_.data(), nullptr,
                           static_cast<std::uint32_t>(draw_buffer_.size()),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_timer_set_period(lv_display_get_refr_timer(display_), config_.refreshPeriodMs);
    return true;
}

void LvglSt7789Display::flushCallback(lv_display_t* display,
                                      const lv_area_t* area,
                                      std::uint8_t* pixels)
{
    auto* self = static_cast<LvglSt7789Display*>(lv_display_get_user_data(display));
    self->flush(area, pixels);
    lv_display_flush_ready(display);
}

void LvglSt7789Display::flush(const lv_area_t* area, std::uint8_t* pixels)
{
    // Keep the first transport failure sticky until the main loop observes it.
    // A later successful area flush in the same LVGL refresh cycle must not
    // hide an earlier SPI failure.
    if (last_status_ != bsp::Status::ok) return;

    const std::int32_t width = area->x2 - area->x1 + 1;
    const std::int32_t height = area->y2 - area->y1 + 1;
    if (width <= 0 || height <= 0) return;

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t byte_count = pixel_count * 2U;
    const auto startedAt = std::chrono::steady_clock::now();
    ++stats_.flushCount;
    stats_.pixelBytes += byte_count;

    // ST7789 consumes RGB565 most-significant byte first. LVGL explicitly
    // permits the partial draw buffer to be byte-swapped in place in the
    // synchronous flush callback. Its helper swaps pairs in 32-bit batches,
    // avoiding both a second buffer and a scalar byte-copy loop.
    lv_draw_sw_rgb565_swap(pixels, static_cast<std::uint32_t>(pixel_count));

    auto status = panel_.setAddressWindow(static_cast<std::uint16_t>(area->x1),
                                          static_cast<std::uint16_t>(area->y1),
                                          static_cast<std::uint16_t>(area->x2),
                                          static_cast<std::uint16_t>(area->y2));
    if (status == bsp::Status::ok) {
        status = panel_.writePixelBytes(pixels, byte_count);
    }
    if (status != bsp::Status::ok) {
        last_status_ = status;
    }
    else {
        ++stats_.successfulFlushCount;
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startedAt);
    const auto elapsedUs = static_cast<std::uint64_t>(
        std::max<std::int64_t>(0, elapsed.count()));
    stats_.totalFlushTimeUs += elapsedUs;
    stats_.maxFlushTimeUs = std::max(stats_.maxFlushTimeUs, elapsedUs);
}

} // namespace display
