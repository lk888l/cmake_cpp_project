#include "input/button_state_machine.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace gpio_button::detail {
namespace {

void append(std::vector<ButtonEvent>& destination,
            std::vector<ButtonEvent> source)
{
    destination.insert(destination.end(),
                       std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
}

} // namespace

ButtonStateMachine::ButtonStateMachine(ButtonConfig config)
    : config_(std::move(config))
{
    if (config_.id.empty() || config_.chip_path.empty() ||
        config_.debounce < MonotonicDuration::zero() ||
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
    pressed_at_.reset(); // Do not synthesize a gesture started before launch.
    last_click_release_.reset();
    pending_edge_.reset();
}

std::vector<ButtonEvent> ButtonStateMachine::processEdge(
    bool pressed, MonotonicTime timestamp)
{
    if (!initialized_) {
        setInitialLevel(pressed, timestamp);
        return {};
    }

    std::vector<ButtonEvent> output;

    // epoll dispatches GPIO records before timer readiness. If an older
    // debounce deadline has already elapsed, commit it before this newer edge.
    if (pending_edge_ && timestamp >= pending_edge_->deadline) {
        const auto pending = *pending_edge_;
        pending_edge_.reset();
        append(output, commitEdge(pending.pressed, pending.timestamp));
    }

    if (pressed == pressed_) {
        // The raw level returned to the stable state before the debounce
        // deadline, so the pending edge was contact bounce.
        pending_edge_.reset();
        return output;
    }

    if (last_edge_ && timestamp - *last_edge_ < config_.debounce) {
        pending_edge_ = PendingEdge{
            .pressed = pressed,
            .timestamp = timestamp,
            .deadline = *last_edge_ + config_.debounce,
        };
        return output;
    }

    append(output, commitEdge(pressed, timestamp));
    return output;
}

std::vector<ButtonEvent> ButtonStateMachine::processTimers(MonotonicTime now)
{
    std::vector<ButtonEvent> output;

    if (pending_edge_ && now >= pending_edge_->deadline) {
        const auto pending = *pending_edge_;
        pending_edge_.reset();
        append(output, commitEdge(pending.pressed, pending.timestamp));
    }

    if (pressed_ && pressed_at_ && !long_press_sent_ &&
        now >= *pressed_at_ + config_.long_press) {
        long_press_sent_ = true;
        output.push_back(event(ButtonEventType::LongPress,
                               *pressed_at_ + config_.long_press,
                               config_.long_press));
    }

    if (last_click_release_ &&
        now >= *last_click_release_ + config_.double_click_window) {
        last_click_release_.reset();
    }
    return output;
}

std::optional<MonotonicTime> ButtonStateMachine::nextDeadline() const
{
    std::optional<MonotonicTime> result;
    const auto consider = [&result](std::optional<MonotonicTime> candidate) {
        if (candidate && (!result || *candidate < *result)) {
            result = candidate;
        }
    };

    if (pending_edge_) {
        consider(pending_edge_->deadline);
    }
    if (pressed_ && pressed_at_ && !long_press_sent_) {
        consider(*pressed_at_ + config_.long_press);
    }
    if (last_click_release_) {
        consider(*last_click_release_ + config_.double_click_window);
    }
    return result;
}

std::vector<ButtonEvent> ButtonStateMachine::commitEdge(
    bool pressed, MonotonicTime timestamp)
{
    if (pressed == pressed_) {
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

    const auto held = pressed_at_
                          ? std::max(MonotonicDuration::zero(),
                                     timestamp - *pressed_at_)
                          : MonotonicDuration::zero();

    // Preserve the inclusive long-press boundary when release and timer
    // readiness occur together.
    if (pressed_at_ && !long_press_sent_ && held >= config_.long_press) {
        long_press_sent_ = true;
        output.push_back(event(ButtonEventType::LongPress,
                               *pressed_at_ + config_.long_press,
                               config_.long_press));
    }

    output.push_back(event(ButtonEventType::Release, timestamp, held));
    if (pressed_at_ && !long_press_sent_) {
        output.push_back(event(ButtonEventType::Click, timestamp, held));
        if (last_click_release_ &&
            timestamp - *last_click_release_ <= config_.double_click_window) {
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

ButtonEvent ButtonStateMachine::event(ButtonEventType type, MonotonicTime time,
                                      MonotonicDuration held) const
{
    return ButtonEvent{
        .id = config_.id,
        .type = type,
        .timestamp = time,
        .held_for = held,
    };
}

} // namespace gpio_button::detail
