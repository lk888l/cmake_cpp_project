#pragma once

#include <atomic>
#include <cstdint>

#include "display/oled_canvas.hpp"
#include "display/oled_display.hpp"
#include "system_state.hpp"

namespace app {

enum class DemoMode : std::uint8_t { dashboard, graphics, grayscale };

struct OledDemoConfig {
    DemoMode mode = DemoMode::dashboard;
    std::uint32_t period_ms = 1000;
    bool once = false;
};

class OledDemoApp {
public:
    OledDemoApp(display::OledDisplay& display, OledDemoConfig config);
    display::DisplayStatus run(const std::atomic_bool& running);

private:
    void drawDashboard(const SystemSnapshot& state);
    void drawGraphics();
    void drawGrayscale();

    display::OledDisplay& display_;
    display::OledCanvas canvas_;
    OledDemoConfig config_;
    SystemState system_state_;
};

} // namespace app

