#include "button_state_machine.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace gpio_button::detail {

ButtonStateMachine::ButtonStateMachine(ButtonConfig config) : config_(std::move(config))
{
    if (config_.id.empty() || config_.debounce < MonotonicDuration::zero() ||
        config_.long_press <= MonotonicDuration::zero() ||
        config_.double_click_window <= MonotonicDuration::zero()) {
        throw std::invalid_argument("invalid button configuration");
    }
}

void ButtonStateMachine::setInitialLevel(bool pressed, MonotonicTime timestamp)
{
    initialized_ = true;
    pressed_ = pressed;
    long_press_sent_ = false;
    last_edge_ = timestamp;
    pressed_at_.reset(); // Do not synthesize a gesture that started before this process.
    last_click_release_.reset();
}

std::vector<ButtonEvent> ButtonStateMachine::processEdge(bool pressed, MonotonicTime timestamp)
{
    if (!initialized_) {
        setInitialLevel(pressed, timestamp);
        return {};
    }
    if (pressed == pressed_ || (last_edge_ && timestamp - *last_edge_ < config_.debounce)) {
        return {};
    }
    last_edge_ = timestamp;
    pressed_ = pressed;
    std::vector<ButtonEvent> output;
    if (pressed) {
        pressed_at_ = timestamp;
        long_press_sent_ = false;
        output.push_back(event(ButtonEventType::Press, timestamp));
        return output;
    }

    const auto held = pressed_at_ ? std::max(MonotonicDuration::zero(), timestamp - *pressed_at_)
                                  : MonotonicDuration::zero();
    // Edge events are dispatched before timer expirations. Preserve the exact
    // long-press boundary even when release and timer readiness coincide.
    if (pressed_at_ && !long_press_sent_ && held >= config_.long_press) {
        long_press_sent_ = true;
        output.push_back(event(ButtonEventType::LongPress, *pressed_at_ + config_.long_press,
                               config_.long_press));
    }
    output.push_back(event(ButtonEventType::Release, timestamp, held));
    if (pressed_at_ && !long_press_sent_) {
        output.push_back(event(ButtonEventType::Click, timestamp, held));
        if (last_click_release_ && timestamp - *last_click_release_ <= config_.double_click_window) {
            output.push_back(event(ButtonEventType::DoubleClick, timestamp, held));
            last_click_release_.reset();
        } else {
            last_click_release_ = timestamp;
        }
    }
    pressed_at_.reset();
    long_press_sent_ = false;
    return output;
}

std::vector<ButtonEvent> ButtonStateMachine::processTimers(MonotonicTime now)
{
    std::vector<ButtonEvent> output;
    if (pressed_ && pressed_at_ && !long_press_sent_ && now >= *pressed_at_ + config_.long_press) {
        long_press_sent_ = true;
        output.push_back(event(ButtonEventType::LongPress, *pressed_at_ + config_.long_press,
                               config_.long_press));
    }
    if (last_click_release_ && now >= *last_click_release_ + config_.double_click_window) {
        last_click_release_.reset();
    }
    return output;
}

std::optional<MonotonicTime> ButtonStateMachine::nextDeadline() const
{
    std::optional<MonotonicTime> result;
    if (pressed_ && pressed_at_ && !long_press_sent_) {
        result = *pressed_at_ + config_.long_press;
    }
    if (last_click_release_) {
        const auto click_deadline = *last_click_release_ + config_.double_click_window;
        if (!result || click_deadline < *result) result = click_deadline;
    }
    return result;
}

ButtonEvent ButtonStateMachine::event(ButtonEventType type, MonotonicTime time, MonotonicDuration held) const
{
    return ButtonEvent{.id = config_.id, .type = type, .timestamp = time, .held_for = held};
}

} // namespace gpio_button::detail
