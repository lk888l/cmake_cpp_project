#include "input/three_key_lvgl_input.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

using namespace std::chrono_literals;

namespace {

using gpio_button::ButtonEvent;
using gpio_button::ButtonEventType;
using gpio_button::MonotonicDuration;
using gpio_button::MonotonicTime;
using input::InputAction;
using input::PhysicalButton;
using input::ThreeKeyLvglInput;

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

ButtonEvent event(ButtonEventType type, long long timestamp_ms,
                  long long held_ms = 0)
{
    return ButtonEvent{
        .id = "test",
        .type = type,
        .timestamp = at(timestamp_ms),
        .held_for = std::chrono::milliseconds{held_ms},
    };
}

void requireActions(std::string_view test,
                    const std::vector<InputAction>& actual,
                    std::initializer_list<InputAction> expected)
{
    require(actual.size() == expected.size(), test, "action count mismatch");
    std::size_t index = 0;
    for (const auto action : expected) {
        require(actual[index] == action, test, "action order mismatch");
        ++index;
    }
}

void queueBoundaryAndTelemetry()
{
    constexpr auto test = "queue and telemetry";
    std::vector<InputAction> actions;
    using Telemetry =
        std::tuple<PhysicalButton, ButtonEventType, std::chrono::milliseconds>;
    std::vector<Telemetry> telemetry;
    ThreeKeyLvglInput router(
        {}, [&](InputAction action) { actions.push_back(action); },
        [&](PhysicalButton button, ButtonEventType type,
            std::chrono::milliseconds held) {
            telemetry.emplace_back(button, type, held);
        });

    router.push(PhysicalButton::Up, event(ButtonEventType::Press, 0));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Press, 1));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Release, 51, 50));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Click, 51, 50));

    require(actions.empty() && telemetry.empty(), test,
            "push must not call main-thread sinks");
    require(router.pending() == 4, test, "queued event count mismatch");
    require(router.pump(at(51)) == 4, test, "generated action count mismatch");
    requireActions(test, actions,
                   {InputAction::Previous, InputAction::ConfirmPressed,
                    InputAction::ConfirmReleased, InputAction::Activate});
    require(telemetry.size() == 4, test, "telemetry event count mismatch");

    const auto confirm = router.snapshot(PhysicalButton::Confirm, at(100));
    require(!confirm.pressed && confirm.press_count == 1 &&
                confirm.last_event == ButtonEventType::Click &&
                confirm.held_for == 50ms,
            test, "confirm snapshot mismatch");
}

void repeatTimingAndRelease()
{
    constexpr auto test = "repeat timing";
    std::vector<InputAction> actions;
    ThreeKeyLvglInput router(
        {.repeat_delay = 400ms, .repeat_period = 100ms},
        [&](InputAction action) { actions.push_back(action); });

    router.push(PhysicalButton::Up, event(ButtonEventType::Press, 0));
    require(router.pump(at(0)) == 1, test, "press should move immediately");
    require(router.pump(at(399)) == 0, test, "repeat started too early");
    require(router.pump(at(400)) == 1, test,
            "first repeat missing at delay boundary");
    require(router.pump(at(499)) == 0, test, "repeat period was too short");
    require(router.pump(at(500)) == 1, test, "second repeat missing");

    router.push(PhysicalButton::Up,
                event(ButtonEventType::Release, 550, 550));
    require(router.pump(at(550)) == 0, test,
            "release must stop repeat immediately");
    require(router.pump(at(1000)) == 0, test,
            "released direction repeated later");
    requireActions(test, actions,
                   {InputAction::Previous, InputAction::Previous,
                    InputAction::Previous});

    const auto up = router.snapshot(PhysicalButton::Up, at(1000));
    require(!up.pressed && up.press_count == 1 && up.held_for == 550ms,
            test, "direction snapshot mismatch");
}

void stalledPumpDoesNotBurst()
{
    constexpr auto test = "no catch-up burst";
    std::vector<InputAction> actions;
    ThreeKeyLvglInput router(
        {}, [&](InputAction action) { actions.push_back(action); });
    router.push(PhysicalButton::Down, event(ButtonEventType::Press, 0));
    require(router.pump(at(0)) == 1, test, "initial action missing");
    require(router.pump(at(2000)) == 1, test,
            "a delayed frame must emit one repeat only");
    require(actions.size() == 2, test, "repeat burst was generated");
}

void oppositeDirectionsPauseAndResume()
{
    constexpr auto test = "opposite direction conflict";
    std::vector<InputAction> actions;
    ThreeKeyLvglInput router(
        {}, [&](InputAction action) { actions.push_back(action); });

    router.push(PhysicalButton::Up, event(ButtonEventType::Press, 0));
    (void)router.pump(at(0));
    router.push(PhysicalButton::Down, event(ButtonEventType::Press, 100));
    (void)router.pump(at(100));
    require(router.pump(at(500)) == 0, test,
            "both directions held should pause repeat");

    router.push(PhysicalButton::Up,
                event(ButtonEventType::Release, 600, 600));
    require(router.pump(at(600)) == 0, test,
            "resume must wait one repeat period");
    require(router.pump(at(699)) == 0, test, "resume occurred too early");
    require(router.pump(at(700)) == 1, test, "remaining key did not resume");

    router.push(PhysicalButton::Down,
                event(ButtonEventType::Release, 750, 650));
    (void)router.pump(at(750));
    requireActions(test, actions,
                   {InputAction::Previous, InputAction::Next,
                    InputAction::Next});
}

void confirmClickAndLongPressAreExclusive()
{
    constexpr auto test = "confirm gesture exclusivity";
    std::vector<InputAction> actions;
    ThreeKeyLvglInput router(
        {}, [&](InputAction action) { actions.push_back(action); });

    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Press, 0));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Release, 80, 80));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Click, 80, 80));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::DoubleClick, 80, 80));
    (void)router.pump(at(80));
    requireActions(test, actions,
                   {InputAction::ConfirmPressed,
                    InputAction::ConfirmReleased, InputAction::Activate});

    actions.clear();
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Press, 200));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::LongPress, 800, 600));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Release, 850, 650));
    // Even a malformed upstream Click after LongPress must not activate.
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::Click, 850, 650));
    router.push(PhysicalButton::Confirm,
                event(ButtonEventType::DoubleClick, 850, 650));
    (void)router.pump(at(850));
    requireActions(test, actions,
                   {InputAction::ConfirmPressed, InputAction::Back,
                    InputAction::ConfirmReleased});
}

void unpairedReleaseAndWorkerPushAreSafe()
{
    constexpr auto test = "worker boundary";
    std::vector<InputAction> actions;
    std::size_t telemetry_count = 0;
    ThreeKeyLvglInput router(
        {}, [&](InputAction action) { actions.push_back(action); },
        [&](PhysicalButton, ButtonEventType, std::chrono::milliseconds) {
            ++telemetry_count;
        });

    std::jthread worker([&] {
        router.push(PhysicalButton::Confirm,
                    event(ButtonEventType::Release, 10));
        router.push(PhysicalButton::Up,
                    event(ButtonEventType::Press, 20));
        router.push(PhysicalButton::Up,
                    event(ButtonEventType::Release, 70, 50));
    });
    worker.join();

    require(actions.empty() && telemetry_count == 0, test,
            "worker push invoked a main-thread callback");
    require(router.pending() == 3, test, "worker events were lost");
    (void)router.pump(at(70));
    requireActions(test, actions, {InputAction::Previous});
    require(telemetry_count == 3, test, "telemetry queue order was lost");
}

void validation()
{
    constexpr auto test = "router validation";
    bool threw = false;
    try {
        ThreeKeyLvglInput invalid(
            {.repeat_delay = 0ms, .repeat_period = 100ms});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, test, "zero repeat delay should be rejected");

    ThreeKeyLvglInput router;
    threw = false;
    try {
        router.push(static_cast<PhysicalButton>(99),
                    event(ButtonEventType::Press, 0));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, test, "invalid physical button should be rejected");
}

} // namespace

int main()
{
    queueBoundaryAndTelemetry();
    repeatTimingAndRelease();
    stalledPumpDoesNotBurst();
    oppositeDirectionsPauseAndResume();
    confirmClickAndLongPressAreExclusive();
    unpairedReleaseAndWorkerPushAreSafe();
    validation();
    std::cout << "three_key_lvgl_input_tests passed\n";
}
