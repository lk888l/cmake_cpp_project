#pragma once

#include "hardware/st7789.hpp"
#include "lvgl.h"

#include <cstdint>
#include <vector>

namespace display {

struct LvglDisplayConfig final {
    std::uint16_t bufferLines{24};
    std::uint32_t refreshPeriodMs{33};
};

struct DisplayStats final {
    std::uint64_t flushCount{};
    std::uint64_t successfulFlushCount{};
    std::uint64_t pixelBytes{};
    std::uint64_t totalFlushTimeUs{};
    std::uint64_t maxFlushTimeUs{};
};

class LvglSt7789Display {
public:
    explicit LvglSt7789Display(hardware::St7789& panel, LvglDisplayConfig config = {});
    ~LvglSt7789Display();
    bool init();
    [[nodiscard]] lv_display_t* handle() const { return display_; }
    [[nodiscard]] bsp::Status lastStatus() const { return last_status_; }
    [[nodiscard]] DisplayStats stats() const noexcept { return stats_; }

private:
    static void flushCallback(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels);
    void flush(const lv_area_t* area, const std::uint8_t* pixels);

    hardware::St7789& panel_;
    LvglDisplayConfig config_;
    lv_display_t* display_{nullptr};
    std::vector<std::uint8_t> draw_buffer_;
    std::vector<std::uint8_t> transmit_buffer_;
    bsp::Status last_status_{bsp::Status::ok};
    DisplayStats stats_{};
};

} // namespace display
