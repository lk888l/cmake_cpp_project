#include "oled_demo_app.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

namespace app {

OledDemoApp::OledDemoApp(display::OledDisplay& display, OledDemoConfig config)
    : display_(display), canvas_(display.nativeHandle()), config_(config) {}

void OledDemoApp::drawDashboard(const SystemSnapshot& state)
{
    char text[32]{};
    canvas_.clear();
    canvas_.setFont(display::Font::small);
    canvas_.drawText(0, 7, "SYSTEM MONITOR");
    canvas_.drawLine(0, 9, 127, 9, 96);

    std::snprintf(text, sizeof(text), "Time: %s", state.time.c_str());
    canvas_.drawText(0, 19, text);
    std::snprintf(text, sizeof(text), "IP: %s", state.ip_address.c_str());
    canvas_.drawText(0, 29, text);

    std::snprintf(text, sizeof(text), "CPU %5.1f%%", state.cpu_percent);
    canvas_.drawText(0, 39, text);
    canvas_.drawProgressBar(61, 32, 66, 8, state.cpu_percent, 176);

    std::snprintf(text, sizeof(text), "MEM %5.1f%%", state.memory_percent);
    canvas_.drawText(0, 51, text);
    canvas_.drawProgressBar(61, 44, 66, 8, state.memory_percent, 112);

    canvas_.drawText(0, 62, "SSD1306 / U8g2", 160);
}

void OledDemoApp::drawGraphics()
{
    canvas_.clear();
    canvas_.setFont(display::Font::small);
    canvas_.drawText(0, 7, "GRAPHICS");
    canvas_.drawRect(0, 10, 34, 22);
    canvas_.fillRect(4, 14, 26, 14, 96);
    canvas_.drawCircle(48, 21, 11);
    canvas_.fillCircle(72, 21, 10, 144);
    canvas_.drawTriangle(88, 31, 104, 11, 122, 31, 224);
    canvas_.drawArc(17, 49, 13, 200, 520, 192);
    canvas_.drawProgressBar(37, 42, 90, 9, 68.0F, 160);
    canvas_.setFont(display::Font::medium);
    canvas_.drawText(39, 63, "Line Circle Arc");
}

void OledDemoApp::drawGrayscale()
{
    canvas_.clear();
    canvas_.setFont(display::Font::small);
    canvas_.drawText(0, 7, "BAYER GRAYSCALE");
    constexpr std::uint8_t levels[] = {0, 42, 85, 128, 170, 213, 255};
    for (std::size_t i = 0; i < sizeof(levels); ++i) {
        const int x = static_cast<int>(i) * 18 + 1;
        canvas_.fillRect(x, 12, 16, 32, levels[i]);
        canvas_.drawRect(x, 12, 16, 32, 255);
    }
    canvas_.drawText(0, 54, "0");
    canvas_.drawText(106, 54, "255");
    canvas_.drawText(0, 63, "Stable spatial dither", 144);
}

display::DisplayStatus OledDemoApp::run(const std::atomic_bool& running)
{
    do {
        switch (config_.mode) {
        case DemoMode::dashboard: drawDashboard(system_state_.sample()); break;
        case DemoMode::graphics: drawGraphics(); break;
        case DemoMode::grayscale: drawGrayscale(); break;
        }
        const auto status = display_.present();
        if (status != display::DisplayStatus::ok || config_.once) { return status; }
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.period_ms));
    } while (running.load());
    return display::DisplayStatus::ok;
}

} // namespace app

