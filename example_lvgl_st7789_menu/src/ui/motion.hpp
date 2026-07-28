#pragma once

#include "lvgl.h"

#include <cstdint>

namespace ui {

struct MotionTiming final {
    static constexpr std::uint32_t FocusMs = 160;
    static constexpr std::uint32_t PressMs = 70;
    static constexpr std::uint32_t ReleaseMs = 130;
    static constexpr std::uint32_t PageMs = 220;
    static constexpr std::uint32_t ValueMs = 140;
    static constexpr std::uint32_t BreathingPeriodMs = 900;
};

/**
 * Retarget one property without allowing stale animations to queue up.
 *
 * The caller owns the animated object and must keep it alive for the animation
 * duration. LVGL uses the (object, setter) pair as the property identity.
 */
void retargetAnimation(void* object,
                       lv_anim_exec_xcb_t setter,
                       std::int32_t from,
                       std::int32_t to,
                       std::uint32_t durationMs,
                       lv_anim_path_cb_t path = lv_anim_path_ease_out) noexcept;

} // namespace ui
