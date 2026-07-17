#include "input/button_state_machine.hpp"

#include <chrono>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace std::chrono_literals;

namespace {

using gpio_button::ButtonConfig;
using gpio_button::ButtonEvent;
using gpio_button::ButtonEventType;
using gpio_button::MonotonicDuration;
using gpio_button::MonotonicTime;
using gpio_button::detail::ButtonStateMachine;

[[noreturn]] void fail(std::string_view test, std::string_view message)
{
    std::cerr << test << ": " << message << '\n';
    std::exit(1);
}

void require(bool condition, std::string_view test, std::string_view message)
{
    if (!condition) {
        fail(test, message);
    }
}

MonotonicTime at(long long milliseconds)
{
    return MonotonicTime{std::chrono::duration_cast<MonotonicDuration>(
        std::chrono::milliseconds{milliseconds})};
}

ButtonConfig config()
{
    return ButtonConfig{
        .id = "button",
        .debounce = 25ms,
        .long_press = 600ms,
        .double_click_window = 250ms,
    };
}

void append(std::vector<ButtonEvent>& destination,
            std::vector<ButtonEvent> source)
{
    destination.insert(destination.end(), source.begin(), source.end());
}

void checkTypes(std::string_view test, const std::vector<ButtonEvent>& events,
                std::initializer_list<ButtonEventType> expected)
{
    require(events.size() == expected.size(), test, "event count mismatch");
    std::size_t index = 0;
    for (const auto type : expected) {
        require(events[index].type == type, test, "event type mismatch");
        ++index;
    }
}

void debounceDeadlineRecoversRelease()
{
    constexpr auto test = "debounce deadline";
    ButtonStateMachine state(config());
    state.setInitialLevel(false, at(0));

    auto press = state.processEdge(true, at(30));
    checkTypes(test, press, {ButtonEventType::Press});
    require(state.processEdge(false, at(40)).empty(), test,
            "early release should wait for debounce deadline");
    require(state.nextDeadline() == at(55), test,
            "debounce deadline was not exposed to timerfd");
    require(state.processTimers(at(54)).empty(), test,
            "pending release committed too early");

    auto release = state.processTimers(at(55));
    checkTypes(test, release,
               {ButtonEventType::Release, ButtonEventType::Click});
    require(release[0].timestamp == at(40), test,
            "release must retain the kernel edge timestamp");
    require(release[0].held_for == 10ms, test,
            "release duration should use physical edge timestamps");
}

void bounceBackCancelsPendingEdge()
{
    constexpr auto test = "bounce cancellation";
    ButtonStateMachine state(config());
    state.setInitialLevel(false, at(0));

    std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30)));
    append(events, state.processEdge(false, at(40)));
    append(events, state.processEdge(true, at(45)));
    append(events, state.processTimers(at(60)));
    append(events, state.processEdge(false, at(100)));

    checkTypes(test, events,
               {ButtonEventType::Press, ButtonEventType::Release,
                ButtonEventType::Click});
    require(events.back().held_for == 70ms, test,
            "bounce must not shorten the real press");
}

void timerLongPressIsExclusive()
{
    constexpr auto test = "timer long press";
    ButtonStateMachine state(config());
    state.setInitialLevel(false, at(0));

    std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30)));
    append(events, state.processTimers(at(629)));
    append(events, state.processTimers(at(630)));
    append(events, state.processTimers(at(800)));
    append(events, state.processEdge(false, at(900)));

    checkTypes(test, events,
               {ButtonEventType::Press, ButtonEventType::LongPress,
                ButtonEventType::Release});
    require(events[1].timestamp == at(630), test,
            "long press timestamp must equal its threshold");
    require(events[1].held_for == 600ms, test,
            "long press duration must equal configured threshold");
    require(events[2].held_for == 870ms, test,
            "release should retain full held duration");
}

void releaseAtLongPressBoundary()
{
    constexpr auto test = "long press boundary";
    ButtonStateMachine state(config());
    state.setInitialLevel(false, at(0));

    std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30)));
    append(events, state.processEdge(false, at(630)));
    checkTypes(test, events,
               {ButtonEventType::Press, ButtonEventType::LongPress,
                ButtonEventType::Release});
}

void doubleClickBoundaryAndExpiry()
{
    constexpr auto test = "double click";
    ButtonStateMachine state(config());
    state.setInitialLevel(false, at(0));

    std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30)));
    append(events, state.processEdge(false, at(80)));
    append(events, state.processEdge(true, at(280)));
    append(events, state.processEdge(false, at(330)));
    checkTypes(test, events,
               {ButtonEventType::Press, ButtonEventType::Release,
                ButtonEventType::Click, ButtonEventType::Press,
                ButtonEventType::Release, ButtonEventType::Click,
                ButtonEventType::DoubleClick});

    ButtonStateMachine expired(config());
    expired.setInitialLevel(false, at(0));
    events.clear();
    append(events, expired.processEdge(true, at(30)));
    append(events, expired.processEdge(false, at(80)));
    append(events, expired.processTimers(at(330)));
    append(events, expired.processEdge(true, at(331)));
    append(events, expired.processEdge(false, at(381)));
    checkTypes(test, events,
               {ButtonEventType::Press, ButtonEventType::Release,
                ButtonEventType::Click, ButtonEventType::Press,
                ButtonEventType::Release, ButtonEventType::Click});
}

void initialPressedDoesNotInventGesture()
{
    constexpr auto test = "initial pressed";
    ButtonStateMachine state(config());
    state.setInitialLevel(true, at(0));

    require(state.processTimers(at(1000)).empty(), test,
            "startup-held button must not synthesize long press");
    auto release = state.processEdge(false, at(1100));
    checkTypes(test, release, {ButtonEventType::Release});
    require(release[0].held_for == MonotonicDuration::zero(), test,
            "unknown startup duration must remain zero");
}

void polarityAndValidation()
{
    constexpr auto test = "polarity and validation";
    auto low = config();
    auto high = config();
    high.active_low = false;
    require(gpio_button::isPressedLevel(low, false) &&
                !gpio_button::isPressedLevel(low, true) &&
                !gpio_button::isPressedLevel(high, false) &&
                gpio_button::isPressedLevel(high, true),
            test, "logical polarity conversion failed");

    auto invalid = config();
    invalid.id.clear();
    bool threw = false;
    try {
        ButtonStateMachine ignored(invalid);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, test, "empty id should be rejected");

    invalid = config();
    invalid.debounce = -1ms;
    threw = false;
    try {
        ButtonStateMachine ignored(invalid);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, test, "negative debounce should be rejected");
}

} // namespace

int main()
{
    debounceDeadlineRecoversRelease();
    bounceBackCancelsPendingEdge();
    timerLongPressIsExclusive();
    releaseAtLongPressBoundary();
    doubleClickBoundaryAndExpiry();
    initialPressedDoesNotInventGesture();
    polarityAndValidation();
    std::cout << "button_state_machine_tests passed\n";
}
