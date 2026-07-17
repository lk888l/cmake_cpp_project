#include "input/three_key_lvgl_input.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace input {

ThreeKeyLvglInput::ThreeKeyLvglInput(InputRouterConfig config,
                                     ActionSink action_sink,
                                     TelemetryCallback telemetry_callback)
    : config_(config),
      action_sink_(std::move(action_sink)),
      telemetry_callback_(std::move(telemetry_callback))
{
    if (config_.repeat_delay <= gpio_button::MonotonicDuration::zero() ||
        config_.repeat_period <= gpio_button::MonotonicDuration::zero()) {
        throw std::invalid_argument("repeat delay and period must be positive");
    }
}

void ThreeKeyLvglInput::setActionSink(ActionSink action_sink)
{
    action_sink_ = std::move(action_sink);
}

void ThreeKeyLvglInput::setTelemetryCallback(
    TelemetryCallback telemetry_callback)
{
    telemetry_callback_ = std::move(telemetry_callback);
}

void ThreeKeyLvglInput::push(PhysicalButton button,
                             const gpio_button::ButtonEvent& event)
{
    push(button, event.type, event.timestamp, event.held_for);
}

void ThreeKeyLvglInput::push(PhysicalButton button,
                             gpio_button::ButtonEventType type,
                             gpio_button::MonotonicTime timestamp,
                             gpio_button::MonotonicDuration held_for)
{
    (void)indexOf(button); // Reject invalid enum values at the producer boundary.
    std::lock_guard lock(queue_mutex_);
    queue_.push_back(QueuedEvent{
        .button = button,
        .type = type,
        .timestamp = timestamp,
        .held_for = held_for,
    });
}

std::size_t ThreeKeyLvglInput::pump(gpio_button::MonotonicTime current_time)
{
    std::deque<QueuedEvent> local;
    {
        std::lock_guard lock(queue_mutex_);
        local.swap(queue_);
    }

    std::size_t actions = 0;
    for (const auto& event : local) {
        actions += process(event);
    }
    actions += processRepeat(current_time);
    return actions;
}

std::size_t ThreeKeyLvglInput::pump()
{
    return pump(now());
}

std::size_t ThreeKeyLvglInput::pending() const
{
    std::lock_guard lock(queue_mutex_);
    return queue_.size();
}

ButtonSnapshot ThreeKeyLvglInput::snapshot(
    PhysicalButton button, gpio_button::MonotonicTime current_time) const
{
    const auto& state = states_[indexOf(button)];
    auto held = state.held_for;
    if (state.pressed && state.pressed_at) {
        held = std::max(gpio_button::MonotonicDuration::zero(),
                        current_time - *state.pressed_at);
    }
    return ButtonSnapshot{
        .pressed = state.pressed,
        .press_count = state.press_count,
        .last_event = state.last_event,
        .last_event_at = state.last_event_at,
        .held_for = held,
    };
}

ButtonSnapshot ThreeKeyLvglInput::snapshot(PhysicalButton button) const
{
    return snapshot(button, now());
}

gpio_button::MonotonicTime ThreeKeyLvglInput::now()
{
    return gpio_button::MonotonicTime{
        std::chrono::duration_cast<gpio_button::MonotonicDuration>(
            std::chrono::steady_clock::now().time_since_epoch())};
}

std::size_t ThreeKeyLvglInput::indexOf(PhysicalButton button)
{
    switch (button) {
    case PhysicalButton::Up:
        return 0;
    case PhysicalButton::Down:
        return 1;
    case PhysicalButton::Confirm:
        return 2;
    }
    throw std::invalid_argument("invalid physical button");
}

std::size_t ThreeKeyLvglInput::process(const QueuedEvent& event)
{
    auto& state = states_[indexOf(event.button)];
    const bool was_pressed = state.pressed;

    state.last_event = event.type;
    state.last_event_at = event.timestamp;
    if (event.type == gpio_button::ButtonEventType::Press) {
        if (!state.pressed) {
            state.pressed = true;
            ++state.press_count;
            state.pressed_at = event.timestamp;
            state.held_for = gpio_button::MonotonicDuration::zero();
        }
    } else if (event.type == gpio_button::ButtonEventType::Release) {
        state.pressed = false;
        state.held_for = event.held_for;
        if (state.pressed_at && state.held_for == gpio_button::MonotonicDuration::zero()) {
            state.held_for = std::max(gpio_button::MonotonicDuration::zero(),
                                      event.timestamp - *state.pressed_at);
        }
        state.pressed_at.reset();
        state.repeat_at.reset();
    } else if (event.held_for > gpio_button::MonotonicDuration::zero()) {
        state.held_for = event.held_for;
    }

    if (telemetry_callback_) {
        telemetry_callback_(
            event.button, event.type,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                state.held_for));
    }

    std::size_t actions = 0;
    if (event.button == PhysicalButton::Up ||
        event.button == PhysicalButton::Down) {
        if (event.type == gpio_button::ButtonEventType::Press && !was_pressed) {
            state.repeat_at = event.timestamp + config_.repeat_delay;
            actions += emit(event.button == PhysicalButton::Up
                                ? InputAction::Previous
                                : InputAction::Next);
        }
        updateDirectionConflict(event.timestamp);
        return actions;
    }

    switch (event.type) {
    case gpio_button::ButtonEventType::Press:
        if (!was_pressed) {
            confirm_long_press_seen_ = false;
            actions += emit(InputAction::ConfirmPressed);
        }
        break;
    case gpio_button::ButtonEventType::Release:
        if (was_pressed) {
            actions += emit(InputAction::ConfirmReleased);
        }
        break;
    case gpio_button::ButtonEventType::Click:
        if (!confirm_long_press_seen_) {
            actions += emit(InputAction::Activate);
        }
        break;
    case gpio_button::ButtonEventType::LongPress:
        if (!confirm_long_press_seen_) {
            confirm_long_press_seen_ = true;
            actions += emit(InputAction::Back);
        }
        break;
    case gpio_button::ButtonEventType::DoubleClick:
        break;
    }
    return actions;
}

std::size_t ThreeKeyLvglInput::processRepeat(
    gpio_button::MonotonicTime current_time)
{
    const auto& up = states_[indexOf(PhysicalButton::Up)];
    const auto& down = states_[indexOf(PhysicalButton::Down)];
    if (up.pressed == down.pressed) {
        return 0; // Both released or both held: no directional repeat.
    }

    auto& active = states_[indexOf(up.pressed ? PhysicalButton::Up
                                             : PhysicalButton::Down)];
    if (!active.repeat_at || current_time < *active.repeat_at) {
        return 0;
    }

    // Emit at most once per pump and resume from now. A stalled UI thread must
    // not receive a burst of old navigation actions when it recovers.
    active.repeat_at = current_time + config_.repeat_period;
    return emit(up.pressed ? InputAction::Previous : InputAction::Next);
}

std::size_t ThreeKeyLvglInput::emit(InputAction action)
{
    if (action_sink_) {
        action_sink_(action);
    }
    return 1;
}

void ThreeKeyLvglInput::updateDirectionConflict(
    gpio_button::MonotonicTime timestamp)
{
    auto& up = states_[indexOf(PhysicalButton::Up)];
    auto& down = states_[indexOf(PhysicalButton::Down)];
    const bool conflict_now = up.pressed && down.pressed;

    if (direction_conflict_ && !conflict_now) {
        if (up.pressed) {
            up.repeat_at = timestamp + config_.repeat_period;
        } else if (down.pressed) {
            down.repeat_at = timestamp + config_.repeat_period;
        }
    }
    direction_conflict_ = conflict_now;
}

} // namespace input
