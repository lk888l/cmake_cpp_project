#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "bsp/i2c/bsp_i2c.hpp"
#include "display/oled_canvas.hpp"
#include "display/oled_display.hpp"

namespace {

class FakeI2CDevice final : public bsp::I2CDevice {
public:
    bsp::I2CStatus write(const std::uint8_t* data, std::size_t length) override
    {
        if (fail_writes) { return bsp::I2CStatus::io_error; }
        writes.emplace_back(data, data + length);
        return bsp::I2CStatus::ok;
    }

    bsp::I2CStatus writeRead(const std::uint8_t*, std::size_t,
                             std::uint8_t*, std::size_t) override
    {
        return bsp::I2CStatus::io_error;
    }

    bool fail_writes = false;
    std::vector<std::vector<std::uint8_t>> writes;
};

void noDelay(std::uint32_t) {}

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

void expect(bool condition, const std::string& message)
{
    if (!condition) { fail(message); }
}

std::size_t countSetBits(const std::uint8_t* data, std::size_t size)
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < size; ++i) {
        std::uint8_t value = data[i];
        while (value != 0) {
            value = static_cast<std::uint8_t>(value & (value - 1));
            ++count;
        }
    }
    return count;
}

void testInitializationAndTransfer()
{
    FakeI2CDevice device;
    display::OledDisplay oled(device, {display::Rotation::deg0,
                                       display::Controller::ssd1306, 0xCF, noDelay});
    expect(oled.initialize() == display::DisplayStatus::ok, "display initializes");
    expect(oled.isInitialized(), "initialized flag is set");
    expect(oled.width() == 128 && oled.height() == 64, "native dimensions are 128x64");
    expect(oled.framebufferSize() == 1024, "full framebuffer is 1024 bytes");
    expect(!device.writes.empty(), "initialization emits I2C writes");
    for (const auto& packet : device.writes) {
        expect(!packet.empty(), "I2C packet is not empty");
        expect(packet.front() == 0x00 || packet.front() == 0x40,
               "SSD1306 packet has command or data control byte");
    }
}

void testTransportError()
{
    FakeI2CDevice device;
    device.fail_writes = true;
    display::OledDisplay oled(device, {display::Rotation::deg0,
                                       display::Controller::ssd1306, 0xCF, noDelay});
    expect(oled.initialize() == display::DisplayStatus::transport_error,
           "I2C failures propagate to display status");
    expect(!oled.isInitialized(), "failed display stays uninitialized");
}

void testSpatialDither()
{
    FakeI2CDevice device;
    display::OledDisplay oled(device, {display::Rotation::deg0,
                                       display::Controller::ssd1306, 0xCF, noDelay});
    display::OledCanvas canvas(oled.nativeHandle());

    canvas.clear(0);
    expect(countSetBits(oled.framebufferData(), oled.framebufferSize()) == 0,
           "black clears every pixel");
    canvas.clear(255);
    expect(countSetBits(oled.framebufferData(), oled.framebufferSize()) == 8192,
           "full intensity lights every pixel");
    canvas.clear(128);
    expect(countSetBits(oled.framebufferData(), oled.framebufferSize()) == 4608,
           "mid intensity uses deterministic 9/16 Bayer coverage");
}

void testDrawingAndText()
{
    FakeI2CDevice device;
    display::OledDisplay oled(device, {display::Rotation::deg0,
                                       display::Controller::ssd1315, 0xCF, noDelay});
    display::OledCanvas canvas(oled.nativeHandle());
    canvas.clear();
    canvas.drawLine(-20, -20, 140, 70, 128);
    canvas.fillRect(-4, 58, 20, 20, 96);
    canvas.drawCircle(64, 32, 20, 200);
    canvas.drawArc(64, 32, 25, 300, 480, 180);
    canvas.setFont(display::Font::medium);
    expect(canvas.textWidth("OLED") > 0, "text measurement works");
    canvas.drawText(0, 10, "OLED UTF-8", 160);
    const auto lit = countSetBits(oled.framebufferData(), oled.framebufferSize());
    expect(lit > 0 && lit < 8192, "clipped graphics and dithered text draw safely");
}

void testRotation()
{
    FakeI2CDevice device;
    display::OledDisplay oled(device, {display::Rotation::deg90,
                                       display::Controller::ssd1306, 0xCF, noDelay});
    expect(oled.width() == 64 && oled.height() == 128, "90 degree rotation swaps dimensions");
}

} // namespace

int main()
{
    testInitializationAndTransfer();
    testTransportError();
    testSpatialDither();
    testDrawingAndText();
    testRotation();
    std::cout << "All OLED core tests passed\n";
    return 0;
}
