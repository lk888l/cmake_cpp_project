#pragma once

#include "gpio_button/button.hpp"

#include <optional>
#include <vector>

namespace gpio_button::detail {

class ButtonStateMachine {
public:
    explicit ButtonStateMachine(ButtonConfig config);
    void setInitialLevel(bool pressed, MonotonicTime timestamp);
    [[nodiscard]] std::vector<ButtonEvent> processEdge(bool pressed, MonotonicTime timestamp);
    [[nodiscard]] std::vector<ButtonEvent> processTimers(MonotonicTime now);
    [[nodiscard]] std::optional<MonotonicTime> nextDeadline() const;

private:
    [[nodiscard]] ButtonEvent event(ButtonEventType type, MonotonicTime time,
                                    MonotonicDuration held = {}) const;
    ButtonConfig config_;
    bool initialized_{false};
    bool pressed_{false};
    bool long_press_sent_{false};
    std::optional<MonotonicTime> last_edge_;
    std::optional<MonotonicTime> pressed_at_;
    std::optional<MonotonicTime> last_click_release_;
};

} // namespace gpio_button::detail
