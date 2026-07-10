#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gpio_button {

using MonotonicDuration = std::chrono::nanoseconds;
using MonotonicTime = std::chrono::time_point<std::chrono::steady_clock, MonotonicDuration>;

enum class ButtonEventType { Press, Release, Click, DoubleClick, LongPress };

struct ButtonConfig {
    std::string id;
    std::string chip_path{"/dev/gpiochip0"};
    unsigned int line_offset{};
    bool active_low{true};
    MonotonicDuration debounce{std::chrono::milliseconds{25}};
    MonotonicDuration long_press{std::chrono::milliseconds{600}};
    MonotonicDuration double_click_window{std::chrono::milliseconds{250}};
};

[[nodiscard]] constexpr bool isPressedLevel(const ButtonConfig& config, bool physical_level_high) noexcept
{
    return config.active_low ? !physical_level_high : physical_level_high;
}

struct ButtonEvent {
    std::string id;
    ButtonEventType type;
    MonotonicTime timestamp;
    MonotonicDuration held_for{};
};

using ButtonCallback = std::function<void(const ButtonEvent&)>;

/// Linux GPIO manager. Callbacks run serially on its event thread and must not block.
class ButtonManager {
public:
    explicit ButtonManager(std::vector<ButtonConfig> buttons);
    ~ButtonManager();
    ButtonManager(const ButtonManager&) = delete;
    ButtonManager& operator=(const ButtonManager&) = delete;

    void setCallback(ButtonCallback callback);
    void start();
    void stop();
    [[nodiscard]] bool isRunning() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gpio_button
