#pragma once

#include "input/button.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>

namespace input {

enum class PhysicalButton { Up, Down, Confirm };

enum class InputAction {
    Previous,
    Next,
    ConfirmPressed,
    ConfirmReleased,
    Activate,
    Back,
};

struct InputRouterConfig {
    gpio_button::MonotonicDuration repeat_delay{std::chrono::milliseconds{400}};
    gpio_button::MonotonicDuration repeat_period{std::chrono::milliseconds{100}};
};

struct ButtonSnapshot {
    bool pressed{};
    std::uint64_t press_count{};
    std::optional<gpio_button::ButtonEventType> last_event;
    std::optional<gpio_button::MonotonicTime> last_event_at;
    gpio_button::MonotonicDuration held_for{};
};

using ActionSink = std::function<void(InputAction)>;
using TelemetryCallback = std::function<void(
    PhysicalButton, gpio_button::ButtonEventType, std::chrono::milliseconds)>;

/// Thread boundary between the GPIO worker and the LVGL/main thread.
/// push() only appends to a protected queue. pump(), sinks, and snapshots belong
/// to the main thread and therefore never call UI code from the GPIO callback.
class ThreeKeyLvglInput {
public:
    explicit ThreeKeyLvglInput(InputRouterConfig config = {},
                               ActionSink action_sink = {},
                               TelemetryCallback telemetry_callback = {});

    ThreeKeyLvglInput(const ThreeKeyLvglInput&) = delete;
    ThreeKeyLvglInput& operator=(const ThreeKeyLvglInput&) = delete;

    void setActionSink(ActionSink action_sink);
    void setTelemetryCallback(TelemetryCallback telemetry_callback);

    void push(PhysicalButton button, const gpio_button::ButtonEvent& event);
    void push(PhysicalButton button, gpio_button::ButtonEventType type,
              gpio_button::MonotonicTime timestamp,
              gpio_button::MonotonicDuration held_for = {});

    /// Drains all currently queued raw events, then emits at most one due
    /// repeat. Skipping catch-up bursts keeps delayed frames responsive.
    [[nodiscard]] std::size_t pump(gpio_button::MonotonicTime now);
    [[nodiscard]] std::size_t pump();

    [[nodiscard]] std::size_t pending() const;
    [[nodiscard]] ButtonSnapshot snapshot(
        PhysicalButton button, gpio_button::MonotonicTime now) const;
    [[nodiscard]] ButtonSnapshot snapshot(PhysicalButton button) const;

private:
    struct QueuedEvent {
        PhysicalButton button;
        gpio_button::ButtonEventType type;
        gpio_button::MonotonicTime timestamp;
        gpio_button::MonotonicDuration held_for;
    };

    struct ButtonState {
        bool pressed{};
        std::uint64_t press_count{};
        std::optional<gpio_button::ButtonEventType> last_event;
        std::optional<gpio_button::MonotonicTime> last_event_at;
        std::optional<gpio_button::MonotonicTime> pressed_at;
        gpio_button::MonotonicDuration held_for{};
        std::optional<gpio_button::MonotonicTime> repeat_at;
    };

    static gpio_button::MonotonicTime now();
    static std::size_t indexOf(PhysicalButton button);
    std::size_t process(const QueuedEvent& event);
    std::size_t processRepeat(gpio_button::MonotonicTime now);
    std::size_t emit(InputAction action);
    void updateDirectionConflict(gpio_button::MonotonicTime timestamp);

    InputRouterConfig config_;
    ActionSink action_sink_;
    TelemetryCallback telemetry_callback_;
    mutable std::mutex queue_mutex_;
    std::deque<QueuedEvent> queue_;
    std::array<ButtonState, 3> states_{};
    bool direction_conflict_{false};
    bool confirm_long_press_seen_{false};
};

} // namespace input
