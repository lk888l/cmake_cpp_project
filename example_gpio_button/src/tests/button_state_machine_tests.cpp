#include "button_state_machine.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace std::chrono_literals;
using gpio_button::ButtonConfig;
using gpio_button::ButtonEvent;
using gpio_button::ButtonEventType;
using gpio_button::MonotonicDuration;
using gpio_button::MonotonicTime;
using gpio_button::detail::ButtonStateMachine;

namespace {
MonotonicTime at(long long ms) { return MonotonicTime{std::chrono::duration_cast<MonotonicDuration>(std::chrono::milliseconds{ms})}; }
ButtonConfig config() { return ButtonConfig{.id = "button", .debounce = 25ms, .long_press = 600ms, .double_click_window = 250ms}; }
void append(std::vector<ButtonEvent>& to, std::vector<ButtonEvent> from) { to.insert(to.end(), from.begin(), from.end()); }
void check(const char* test, const std::vector<ButtonEvent>& events, std::initializer_list<ButtonEventType> types)
{
    if (events.size() != types.size()) { std::cerr << test << ": count mismatch\n"; std::exit(1); }
    std::size_t i{};
    for (const auto type : types) if (events[i++].type != type) { std::cerr << test << ": type mismatch\n"; std::exit(1); }
}
void debounceAndClick()
{
    ButtonStateMachine state(config()); state.setInitialLevel(false, at(0)); std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30))); append(events, state.processEdge(false, at(40)));
    append(events, state.processEdge(false, at(70)));
    check("debounce", events, {ButtonEventType::Press, ButtonEventType::Release, ButtonEventType::Click});
}
void longPress()
{
    ButtonStateMachine state(config()); state.setInitialLevel(false, at(0)); std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30))); append(events, state.processEdge(false, at(630)));
    check("long press", events, {ButtonEventType::Press, ButtonEventType::LongPress, ButtonEventType::Release});
}
void doubleClick()
{
    ButtonStateMachine state(config()); state.setInitialLevel(false, at(0)); std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30))); append(events, state.processEdge(false, at(80)));
    append(events, state.processEdge(true, at(130))); append(events, state.processEdge(false, at(180)));
    check("double click", events, {ButtonEventType::Press, ButtonEventType::Release, ButtonEventType::Click,
        ButtonEventType::Press, ButtonEventType::Release, ButtonEventType::Click, ButtonEventType::DoubleClick});
}
void expiredWindowAndActiveLow()
{
    ButtonStateMachine state(config()); state.setInitialLevel(false, at(0)); std::vector<ButtonEvent> events;
    append(events, state.processEdge(true, at(30))); append(events, state.processEdge(false, at(80)));
    append(events, state.processTimers(at(330))); append(events, state.processEdge(true, at(360))); append(events, state.processEdge(false, at(410)));
    check("expired double click", events, {ButtonEventType::Press, ButtonEventType::Release, ButtonEventType::Click,
        ButtonEventType::Press, ButtonEventType::Release, ButtonEventType::Click});
    auto low = config(); auto high = config(); high.active_low = false;
    if (!gpio_button::isPressedLevel(low, false) || gpio_button::isPressedLevel(low, true) ||
        gpio_button::isPressedLevel(high, false) || !gpio_button::isPressedLevel(high, true)) std::exit(1);
}
}
int main() { debounceAndClick(); longPress(); doubleClick(); expiredWindowAndActiveLow(); std::cout << "passed\n"; }
