#include "app/menu_model.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void homeCarouselAndBoundary()
{
    app::MenuModel model;
    model.handleAction(input::InputAction::Previous);
    check(model.snapshot().homeSelection == 3, "home wraps backward");
    model.handleAction(input::InputAction::Next);
    check(model.snapshot().homeSelection == 0, "home wraps forward");
    model.handleAction(input::InputAction::Back);
    check(model.snapshot().page == app::MenuPage::Home, "home back stays home");
    check(model.snapshot().boundaryFeedback == 1, "home back records boundary feedback");
}

void arcEditingAndBounds()
{
    app::MenuModel model;
    model.handleAction(input::InputAction::Activate);
    check(model.snapshot().page == app::MenuPage::Arc, "opens arc page");
    model.handleAction(input::InputAction::Activate);
    check(model.snapshot().mode == app::MenuMode::Edit, "enters arc edit");
    for (int i = 0; i < 30; ++i) model.handleAction(input::InputAction::Next);
    check(model.snapshot().arcValue == 100, "arc clamps high");
    for (int i = 0; i < 30; ++i) model.handleAction(input::InputAction::Previous);
    check(model.snapshot().arcValue == 0, "arc clamps low");
    model.handleAction(input::InputAction::Back);
    check(model.snapshot().mode == app::MenuMode::Browse, "back exits edit first");
    model.handleAction(input::InputAction::Back);
    check(model.snapshot().page == app::MenuPage::Home, "second back returns home");
    model.handleAction(input::InputAction::Activate);
    check(model.snapshot().arcValue == 0, "arc value survives page return");
}

void switchesAndAnimation()
{
    app::MenuModel switches;
    switches.handleAction(input::InputAction::Next);
    switches.handleAction(input::InputAction::Activate);
    check(switches.snapshot().page == app::MenuPage::Switches, "opens switches page");
    const bool first = switches.snapshot().switches[0];
    switches.handleAction(input::InputAction::Activate);
    check(switches.snapshot().switches[0] != first, "toggles first switch");
    switches.handleAction(input::InputAction::Next);
    check(switches.snapshot().detailFocus == 1, "moves switch focus");
    const bool second = switches.snapshot().switches[1];
    switches.handleAction(input::InputAction::Activate);
    check(switches.snapshot().switches[1] != second, "toggles second switch");

    app::MenuModel animation;
    animation.handleAction(input::InputAction::Next);
    animation.handleAction(input::InputAction::Next);
    animation.handleAction(input::InputAction::Activate);
    check(animation.snapshot().page == app::MenuPage::Animation, "opens animation page");
    animation.handleAction(input::InputAction::Activate);
    check(animation.snapshot().mode == app::MenuMode::Edit, "enters speed edit");
    animation.handleAction(input::InputAction::Next);
    check(animation.snapshot().animationSpeed == 2, "changes animation speed");
    animation.handleAction(input::InputAction::Activate);
    check(animation.snapshot().mode == app::MenuMode::Browse, "saves speed edit");
    animation.handleAction(input::InputAction::Next);
    const bool playing = animation.snapshot().animationPlaying;
    animation.handleAction(input::InputAction::Activate);
    check(animation.snapshot().animationPlaying != playing, "toggles playback");
}

void everyPageCanReturnToHome()
{
    constexpr app::MenuPage expected[] = {
        app::MenuPage::Arc,
        app::MenuPage::Switches,
        app::MenuPage::Animation,
        app::MenuPage::Buttons,
    };
    for (std::uint8_t index = 0; index < app::MenuModel::HomeItemCount; ++index) {
        app::MenuModel model;
        for (std::uint8_t step = 0; step < index; ++step) {
            model.handleAction(input::InputAction::Next);
        }
        model.handleAction(input::InputAction::Activate);
        check(model.snapshot().page == expected[index], "opens selected detail page");
        model.handleAction(input::InputAction::Back);
        check(model.snapshot().page == app::MenuPage::Home, "detail back pops to home");
        check(model.snapshot().homeSelection == index, "page return preserves Home selection");
    }
}

void pressFeedbackDoesNotChangeModel()
{
    app::MenuModel model;
    const auto before = model.snapshot();
    check(!model.handleAction(input::InputAction::ConfirmPressed), "press is visual only");
    check(!model.handleAction(input::InputAction::ConfirmReleased), "release is visual only");
    check(model.snapshot().homeSelection == before.homeSelection, "press does not navigate");
}

} // namespace

int main()
{
    homeCarouselAndBoundary();
    arcEditingAndBounds();
    switchesAndAnimation();
    everyPageCanReturnToHome();
    pressFeedbackDoesNotChangeModel();
    std::cout << "menu model tests passed\n";
}
