#pragma once

#include "core/config.hpp"
#include "core/io.hpp"
#include "core/layout.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace camera_display {

class St7789 final {
public:
    St7789(SpiBus& spi, OutputPin& dc, OutputPin& reset,
           OutputPin& backlight, DisplayConfig config);
    ~St7789();

    St7789(const St7789&) = delete;
    St7789& operator=(const St7789&) = delete;

    IoStatus initialize();
    IoStatus resetAndInitialize();
    IoStatus clear(std::uint16_t nativeRgb565 = 0);
    IoStatus writeRectangle(const Rect& rectangle,
                            const std::uint16_t* nativeRgb565,
                            std::size_t pixelCount);
    IoStatus setBacklight(bool on);
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

private:
    IoStatus command(std::uint8_t value);
    IoStatus data(const std::uint8_t* bytes, std::size_t size);
    IoStatus commandData(std::uint8_t value,
                         const std::uint8_t* bytes, std::size_t size);
    IoStatus setAddressWindow(const Rect& rectangle);
    IoStatus hardwareReset();

    SpiBus& spi_;
    OutputPin& dc_;
    OutputPin& reset_;
    OutputPin& backlight_;
    DisplayConfig config_;
    std::vector<std::uint8_t> wire_scratch_;
    bool initialized_{};
};

} // namespace camera_display
