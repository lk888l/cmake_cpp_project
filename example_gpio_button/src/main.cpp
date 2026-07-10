#include "gpio_button/button.hpp"

#include <pthread.h>
#include <signal.h>

#include <chrono>
#include <exception>
#include <iostream>
#include <string_view>

namespace {
std::string_view name(gpio_button::ButtonEventType type)
{
    using gpio_button::ButtonEventType;
    switch (type) {
    case ButtonEventType::Press: return "Press";
    case ButtonEventType::Release: return "Release";
    case ButtonEventType::Click: return "Click";
    case ButtonEventType::DoubleClick: return "DoubleClick";
    case ButtonEventType::LongPress: return "LongPress";
    }
    return "Unknown";
}
}

int main()
{
    // Change these three board-specific values to match the schematic/device tree.
    gpio_button::ButtonConfig button{
        .id = "user-button", .chip_path = "/dev/gpiochip1", .line_offset = 25, .active_low = true,
    };
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) return 1;

    try {
        gpio_button::ButtonManager manager({button});
        manager.setCallback([](const gpio_button::ButtonEvent& event) {
            std::cout << event.id << ": " << name(event.type) << " held="
                      << std::chrono::duration_cast<std::chrono::milliseconds>(event.held_for).count() << "ms\n";
        });
        manager.start();
        std::cout << "Watching " << button.chip_path << " line " << button.line_offset << "; Ctrl+C exits.\n";
        int signal{};
        (void)sigwait(&signals, &signal);
        manager.stop();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
