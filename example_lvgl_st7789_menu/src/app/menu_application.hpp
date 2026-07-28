#pragma once

#include "input/three_key_lvgl_input.hpp"
#include "runtime/render_policy.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

struct lv_obj_t;

namespace app {

/**
 * Main-thread LVGL application for the three-key menu.
 *
 * handleAction() and tick() must be called on the LVGL owner thread.  The
 * telemetry method never touches LVGL and may therefore be called from the
 * GPIO thread, although feeding snapshots from the main loop is preferred.
 */
class MenuApplication final {
public:
    MenuApplication();
    ~MenuApplication();

    MenuApplication(const MenuApplication&) = delete;
    MenuApplication& operator=(const MenuApplication&) = delete;
    MenuApplication(MenuApplication&&) = delete;
    MenuApplication& operator=(MenuApplication&&) = delete;

    [[nodiscard]] bool create(lv_obj_t* root, const runtime::RenderPolicy& policy = {});
    [[nodiscard]] bool usesLargeObjectLayers() const noexcept;
    void handleAction(input::InputAction action);
    void tick(std::uint32_t elapsedMs);

    void onButtonTelemetry(input::PhysicalButton button,
                           bool pressed,
                           std::uint32_t pressCount,
                           std::uint32_t heldMs,
                           std::string_view lastEvent = {});

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace app
