#include "st7789.hpp"

#include <array>
#include <chrono>
#include <thread>
#include <utility>

namespace hardware {
namespace {

using namespace std::chrono_literals;

constexpr uint8_t kSwReset = 0x01;
constexpr uint8_t kSleepOut = 0x11;
constexpr uint8_t kNormalOn = 0x13;
constexpr uint8_t kInvertOn = 0x21;
constexpr uint8_t kInvertOff = 0x20;
constexpr uint8_t kDisplayOn = 0x29;
constexpr uint8_t kColumnAddress = 0x2A;
constexpr uint8_t kRowAddress = 0x2B;
constexpr uint8_t kMemoryWrite = 0x2C;
constexpr uint8_t kMemoryAccess = 0x36;
constexpr uint8_t kPixelFormat = 0x3A;

} // namespace

St7789::St7789(bsp::SpiBus& spi,
               bsp::OutputPin& dc,
               bsp::OutputPin& reset,
               bsp::OutputPin& backlight,
               St7789Config config)
    : spi_(spi), dc_(dc), reset_(reset), backlight_(backlight), config_(config)
{
}

bsp::Status St7789::command(uint8_t value)
{
    bsp::Status status = dc_.set(false);
    return status == bsp::Status::ok ? spi_.write(&value, 1) : status;
}

bsp::Status St7789::data(const uint8_t* bytes, size_t length)
{
    bsp::Status status = dc_.set(true);
    return status == bsp::Status::ok ? spi_.write(bytes, length) : status;
}

bsp::Status St7789::commandData(uint8_t command_value, const uint8_t* bytes, size_t length)
{
    bsp::Status status = command(command_value);
    return status == bsp::Status::ok ? data(bytes, length) : status;
}

bsp::Status St7789::init()
{
    if (!spi_.isInitialized() || !dc_.isInitialized()) return bsp::Status::invalid_state;
    if (config_.width == 0 || config_.height == 0) return bsp::Status::invalid_argument;

    backlight_.set(false);
    reset_.set(true);
    std::this_thread::sleep_for(10ms);
    reset_.set(false);
    std::this_thread::sleep_for(20ms);
    reset_.set(true);
    std::this_thread::sleep_for(120ms);

    bsp::Status status = command(kSwReset);
    if (status != bsp::Status::ok) return status;
    std::this_thread::sleep_for(150ms);
    if ((status = command(kSleepOut)) != bsp::Status::ok) return status;
    std::this_thread::sleep_for(120ms);

    const uint8_t pixel_format = 0x55;
    if ((status = commandData(kPixelFormat, &pixel_format, 1)) != bsp::Status::ok) return status;

    uint8_t madctl = config_.bgr ? 0x08 : 0x00;
    switch (config_.rotation_deg) {
    case 0: break;
    case 90: madctl |= 0x60; break;
    case 180: madctl |= 0xC0; break;
    case 270: madctl |= 0xA0; break;
    default: return bsp::Status::invalid_argument;
    }
    if ((status = commandData(kMemoryAccess, &madctl, 1)) != bsp::Status::ok) return status;

    const std::array<uint8_t, 5> porch = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    const std::array<uint8_t, 2> power = {0xA4, 0xA1};
    const std::array<uint8_t, 14> gamma_pos = {
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    const std::array<uint8_t, 14> gamma_neg = {
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
    const std::array<std::pair<uint8_t, uint8_t>, 7> registers = {{
        {0xB7, 0x35}, {0xBB, 0x19}, {0xC0, 0x2C}, {0xC2, 0x01},
        {0xC3, 0x12}, {0xC4, 0x20}, {0xC6, 0x0F}}};

    if ((status = commandData(0xB2, porch.data(), porch.size())) != bsp::Status::ok) return status;
    for (const auto& reg : registers) {
        if ((status = commandData(reg.first, &reg.second, 1)) != bsp::Status::ok) return status;
    }
    if ((status = commandData(0xD0, power.data(), power.size())) != bsp::Status::ok) return status;
    if ((status = commandData(0xE0, gamma_pos.data(), gamma_pos.size())) != bsp::Status::ok) return status;
    if ((status = commandData(0xE1, gamma_neg.data(), gamma_neg.size())) != bsp::Status::ok) return status;
    if ((status = command(config_.invert_colors ? kInvertOn : kInvertOff)) != bsp::Status::ok) return status;
    if ((status = command(kNormalOn)) != bsp::Status::ok) return status;
    std::this_thread::sleep_for(10ms);
    if ((status = command(kDisplayOn)) != bsp::Status::ok) return status;
    std::this_thread::sleep_for(100ms);
    return setBacklight(true);
}

bsp::Status St7789::setBacklight(bool on)
{
    return backlight_.set(on);
}

bsp::Status St7789::setAddressWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    x1 = static_cast<uint16_t>(x1 + config_.x_offset);
    x2 = static_cast<uint16_t>(x2 + config_.x_offset);
    y1 = static_cast<uint16_t>(y1 + config_.y_offset);
    y2 = static_cast<uint16_t>(y2 + config_.y_offset);
    const std::array<uint8_t, 4> columns = {
        static_cast<uint8_t>(x1 >> 8), static_cast<uint8_t>(x1),
        static_cast<uint8_t>(x2 >> 8), static_cast<uint8_t>(x2)};
    const std::array<uint8_t, 4> rows = {
        static_cast<uint8_t>(y1 >> 8), static_cast<uint8_t>(y1),
        static_cast<uint8_t>(y2 >> 8), static_cast<uint8_t>(y2)};

    bsp::Status status = commandData(kColumnAddress, columns.data(), columns.size());
    if (status != bsp::Status::ok) return status;
    if ((status = commandData(kRowAddress, rows.data(), rows.size())) != bsp::Status::ok) return status;
    return command(kMemoryWrite);
}

bsp::Status St7789::writePixelBytes(const uint8_t* bytes, size_t length)
{
    return data(bytes, length);
}

} // namespace hardware
