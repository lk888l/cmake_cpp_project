#pragma once

#include "input/three_key_lvgl_input.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace app {

enum class MenuPage : std::uint8_t {
    Home,
    Arc,
    Switches,
    Animation,
    Buttons,
};

enum class MenuMode : std::uint8_t {
    Browse,
    Edit,
};

struct MenuSnapshot {
    MenuPage page{MenuPage::Home};
    MenuMode mode{MenuMode::Browse};
    std::uint8_t homeSelection{0};
    std::uint8_t detailFocus{0};
    std::uint8_t arcValue{50};
    std::array<bool, 2> switches{true, true};
    std::uint8_t animationSpeed{1};
    bool animationPlaying{true};
    std::uint32_t revision{0};
    std::uint32_t boundaryFeedback{0};

    [[nodiscard]] constexpr bool operator==(const MenuSnapshot&) const = default;
};

/**
 * Pure menu state machine.  It deliberately contains no LVGL calls so that
 * navigation and edit semantics can be exercised by host-side tests.
 */
class MenuModel final {
public:
    static constexpr std::uint8_t HomeItemCount = 4;
    static constexpr std::uint8_t ArcStep = 5;

    [[nodiscard]] constexpr const MenuSnapshot& snapshot() const noexcept
    {
        return state_;
    }

    /** Returns true when the action changed observable menu state. */
    constexpr bool handleAction(input::InputAction action) noexcept
    {
        const auto before = state_;

        switch (action) {
        case input::InputAction::Previous:
            navigate(-1);
            break;
        case input::InputAction::Next:
            navigate(1);
            break;
        case input::InputAction::Activate:
            activate();
            break;
        case input::InputAction::Back:
            back();
            break;
        case input::InputAction::ConfirmPressed:
        case input::InputAction::ConfirmReleased:
            break;
        }

        if (before != state_) {
            ++state_.revision;
            return true;
        }
        return false;
    }

private:
    static constexpr std::uint8_t wrap(std::uint8_t value, int delta, std::uint8_t count) noexcept
    {
        const int wrapped = (static_cast<int>(value) + delta + static_cast<int>(count))
            % static_cast<int>(count);
        return static_cast<std::uint8_t>(wrapped);
    }

    constexpr void navigate(int delta) noexcept
    {
        if (state_.page == MenuPage::Home) {
            state_.homeSelection = wrap(state_.homeSelection, delta, HomeItemCount);
            return;
        }

        if (state_.mode == MenuMode::Edit) {
            if (state_.page == MenuPage::Arc) {
                const int next = static_cast<int>(state_.arcValue) + delta * ArcStep;
                state_.arcValue = static_cast<std::uint8_t>(next < 0 ? 0 : (next > 100 ? 100 : next));
            }
            else if (state_.page == MenuPage::Animation && state_.detailFocus == 0) {
                state_.animationSpeed = wrap(state_.animationSpeed, delta, 3);
            }
            return;
        }

        switch (state_.page) {
        case MenuPage::Switches:
        case MenuPage::Animation:
            state_.detailFocus = wrap(state_.detailFocus, delta, 2);
            break;
        case MenuPage::Arc:
        case MenuPage::Buttons:
        case MenuPage::Home:
            break;
        }
    }

    constexpr void activate() noexcept
    {
        if (state_.page == MenuPage::Home) {
            state_.page = static_cast<MenuPage>(state_.homeSelection + 1);
            state_.detailFocus = 0;
            state_.mode = MenuMode::Browse;
            return;
        }

        switch (state_.page) {
        case MenuPage::Arc:
            state_.mode = state_.mode == MenuMode::Browse ? MenuMode::Edit : MenuMode::Browse;
            break;
        case MenuPage::Switches:
            state_.switches[state_.detailFocus] = !state_.switches[state_.detailFocus];
            break;
        case MenuPage::Animation:
            if (state_.detailFocus == 0) {
                state_.mode = state_.mode == MenuMode::Browse ? MenuMode::Edit : MenuMode::Browse;
            }
            else {
                state_.animationPlaying = !state_.animationPlaying;
            }
            break;
        case MenuPage::Buttons:
        case MenuPage::Home:
            break;
        }
    }

    constexpr void back() noexcept
    {
        if (state_.page == MenuPage::Home) {
            ++state_.boundaryFeedback;
            return;
        }

        if (state_.mode == MenuMode::Edit) {
            state_.mode = MenuMode::Browse;
            return;
        }

        state_.page = MenuPage::Home;
        state_.detailFocus = 0;
    }

    MenuSnapshot state_{};
};

} // namespace app
