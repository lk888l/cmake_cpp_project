#include "drivers/st7789.hpp"

#include "core/rgb565.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <utility>

namespace camera_display {
namespace {

using namespace std::chrono_literals;

constexpr std::uint8_t kSoftwareReset = 0x01;
constexpr std::uint8_t kSleepOut = 0x11;
constexpr std::uint8_t kNormalOn = 0x13;
constexpr std::uint8_t kInvertOff = 0x20;
constexpr std::uint8_t kInvertOn = 0x21;
constexpr std::uint8_t kDisplayOn = 0x29;
constexpr std::uint8_t kColumnAddress = 0x2A;
constexpr std::uint8_t kRowAddress = 0x2B;
constexpr std::uint8_t kMemoryWrite = 0x2C;
constexpr std::uint8_t kMemoryAccess = 0x36;
constexpr std::uint8_t kPixelFormat = 0x3A;

} // namespace

St7789::St7789(SpiBus& spi, OutputPin& dc, OutputPin& reset,
               OutputPin& backlight, DisplayConfig config)
    : spi_(spi),
      dc_(dc),
      reset_(reset),
      backlight_(backlight),
      config_(std::move(config)),
      wire_scratch_(
          std::max<std::size_t>(2, config_.spi_chunk_bytes & ~std::size_t{1}), 0)
{
}

St7789::~St7789()
{
    shutdown();
}

IoStatus St7789::hardwareReset()
{
    IoStatus status = backlight_.set(false);
    if (status != IoStatus::Ok) return status;
    if ((status = reset_.set(true)) != IoStatus::Ok) return status;
    std::this_thread::sleep_for(10ms);
    if ((status = reset_.set(false)) != IoStatus::Ok) return status;
    std::this_thread::sleep_for(20ms);
    if ((status = reset_.set(true)) != IoStatus::Ok) return status;
    std::this_thread::sleep_for(120ms);
    return IoStatus::Ok;
}

IoStatus St7789::initialize()
{
    if (initialized_) return IoStatus::InvalidState;
    if (!spi_.isOpen() || !dc_.isRequested() || !reset_.isRequested()
        || !backlight_.isRequested()) {
        return IoStatus::InvalidState;
    }
    if (config_.width != 240 || config_.height != 240
        || wire_scratch_.empty()) {
        return IoStatus::InvalidArgument;
    }

    IoStatus status = hardwareReset();
    if (status != IoStatus::Ok) return status;
    if ((status = command(kSoftwareReset)) != IoStatus::Ok) return status;
    std::this_thread::sleep_for(150ms);
    if ((status = command(kSleepOut)) != IoStatus::Ok) return status;
    std::this_thread::sleep_for(120ms);

    const std::uint8_t pixelFormat = 0x55;
    if ((status = commandData(kPixelFormat, &pixelFormat, 1)) != IoStatus::Ok) {
        return status;
    }
    std::uint8_t memoryAccess = config_.bgr ? 0x08 : 0x00;
    switch (config_.rotation) {
    case 0: break;
    case 90: memoryAccess = static_cast<std::uint8_t>(memoryAccess | 0x60); break;
    case 180: memoryAccess = static_cast<std::uint8_t>(memoryAccess | 0xC0); break;
    case 270: memoryAccess = static_cast<std::uint8_t>(memoryAccess | 0xA0); break;
    default: return IoStatus::InvalidArgument;
    }
    if ((status = commandData(kMemoryAccess, &memoryAccess, 1)) != IoStatus::Ok) {
        return status;
    }

    const std::array<std::uint8_t, 5> porch{{0x0C, 0x0C, 0x00, 0x33, 0x33}};
    const std::array<std::uint8_t, 2> power{{0xA4, 0xA1}};
    const std::array<std::uint8_t, 14> gammaPositive{{
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}};
    const std::array<std::uint8_t, 14> gammaNegative{{
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}};
    const std::array<std::pair<std::uint8_t, std::uint8_t>, 7> registers{{
        {0xB7, 0x35}, {0xBB, 0x19}, {0xC0, 0x2C}, {0xC2, 0x01},
        {0xC3, 0x12}, {0xC4, 0x20}, {0xC6, 0x0F}}};

    if ((status = commandData(0xB2, porch.data(), porch.size())) != IoStatus::Ok) {
        return status;
    }
    for (const auto& item : registers) {
        if ((status = commandData(item.first, &item.second, 1)) != IoStatus::Ok) {
            return status;
        }
    }
    if ((status = commandData(0xD0, power.data(), power.size())) != IoStatus::Ok
        || (status = commandData(0xE0, gammaPositive.data(),
                                 gammaPositive.size())) != IoStatus::Ok
        || (status = commandData(0xE1, gammaNegative.data(),
                                 gammaNegative.size())) != IoStatus::Ok
        || (status = command(config_.invert_colors
                                 ? kInvertOn : kInvertOff)) != IoStatus::Ok
        || (status = command(kNormalOn)) != IoStatus::Ok) {
        return status;
    }
    std::this_thread::sleep_for(10ms);
    if ((status = command(kDisplayOn)) != IoStatus::Ok) return status;
    std::this_thread::sleep_for(100ms);
    initialized_ = true;
    if ((status = clear()) != IoStatus::Ok) {
        initialized_ = false;
        return status;
    }
    return setBacklight(true);
}

IoStatus St7789::resetAndInitialize()
{
    initialized_ = false;
    return initialize();
}

IoStatus St7789::command(std::uint8_t value)
{
    IoStatus status = dc_.set(false);
    return status == IoStatus::Ok ? spi_.write(&value, 1) : status;
}

IoStatus St7789::data(const std::uint8_t* bytes, std::size_t size)
{
    IoStatus status = dc_.set(true);
    return status == IoStatus::Ok ? spi_.write(bytes, size) : status;
}

IoStatus St7789::commandData(std::uint8_t value,
                             const std::uint8_t* bytes, std::size_t size)
{
    const IoStatus status = command(value);
    return status == IoStatus::Ok ? data(bytes, size) : status;
}

IoStatus St7789::setAddressWindow(const Rect& rectangle)
{
    if (rectangle.width == 0 || rectangle.height == 0
        || rectangle.x + rectangle.width > config_.width
        || rectangle.y + rectangle.height > config_.height) {
        return IoStatus::InvalidArgument;
    }
    const auto x1 = static_cast<std::uint16_t>(rectangle.x + config_.x_offset);
    const auto y1 = static_cast<std::uint16_t>(rectangle.y + config_.y_offset);
    const auto x2 = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(rectangle.x)
        + rectangle.width - 1U + config_.x_offset);
    const auto y2 = static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(rectangle.y)
        + rectangle.height - 1U + config_.y_offset);
    const std::array<std::uint8_t, 4> columns{{
        static_cast<std::uint8_t>(x1 >> 8U), static_cast<std::uint8_t>(x1),
        static_cast<std::uint8_t>(x2 >> 8U), static_cast<std::uint8_t>(x2)}};
    const std::array<std::uint8_t, 4> rows{{
        static_cast<std::uint8_t>(y1 >> 8U), static_cast<std::uint8_t>(y1),
        static_cast<std::uint8_t>(y2 >> 8U), static_cast<std::uint8_t>(y2)}};
    IoStatus status = commandData(kColumnAddress, columns.data(), columns.size());
    if (status != IoStatus::Ok) return status;
    if ((status = commandData(kRowAddress, rows.data(), rows.size()))
        != IoStatus::Ok) {
        return status;
    }
    return command(kMemoryWrite);
}

IoStatus St7789::writeRectangle(const Rect& rectangle,
                                const std::uint16_t* nativeRgb565,
                                std::size_t pixelCount)
{
    if (!initialized_) return IoStatus::InvalidState;
    const std::size_t expected =
        static_cast<std::size_t>(rectangle.width) * rectangle.height;
    if (nativeRgb565 == nullptr || pixelCount != expected) {
        return IoStatus::InvalidArgument;
    }
    IoStatus status = setAddressWindow(rectangle);
    if (status != IoStatus::Ok) return status;

    std::size_t offset{};
    const std::size_t scratchPixels = wire_scratch_.size() / 2U;
    while (offset < pixelCount) {
        const std::size_t count = std::min(scratchPixels, pixelCount - offset);
        encodeRgb565BigEndian(nativeRgb565 + offset, wire_scratch_.data(), count);
        if ((status = data(wire_scratch_.data(), count * 2U)) != IoStatus::Ok) {
            return status;
        }
        offset += count;
    }
    return IoStatus::Ok;
}

IoStatus St7789::clear(std::uint16_t nativeRgb565)
{
    if (!initialized_) return IoStatus::InvalidState;
    const Rect entire{0, 0, config_.width, config_.height};
    IoStatus status = setAddressWindow(entire);
    if (status != IoStatus::Ok) return status;
    const std::uint8_t high = static_cast<std::uint8_t>(nativeRgb565 >> 8U);
    const std::uint8_t low = static_cast<std::uint8_t>(nativeRgb565);
    for (std::size_t index = 0; index < wire_scratch_.size(); index += 2U) {
        wire_scratch_[index] = high;
        wire_scratch_[index + 1U] = low;
    }
    std::size_t bytesRemaining =
        static_cast<std::size_t>(config_.width) * config_.height * 2U;
    while (bytesRemaining != 0) {
        const std::size_t count = std::min(bytesRemaining, wire_scratch_.size());
        if ((status = data(wire_scratch_.data(), count)) != IoStatus::Ok) {
            return status;
        }
        bytesRemaining -= count;
    }
    return IoStatus::Ok;
}

IoStatus St7789::setBacklight(bool on)
{
    return backlight_.set(on);
}

void St7789::shutdown() noexcept
{
    if (backlight_.isRequested()) (void)backlight_.set(false);
    initialized_ = false;
}

} // namespace camera_display
