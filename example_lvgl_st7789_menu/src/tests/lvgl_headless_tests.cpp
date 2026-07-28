#include "app/menu_application.hpp"
#include "runtime/lvgl_memory_pool.hpp"
#include "runtime/render_policy.hpp"

#include "lvgl.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

std::size_t flushCount{};
std::size_t flushedPixels{};

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t*)
{
    ++flushCount;
    flushedPixels += static_cast<std::size_t>(lv_area_get_width(area))
        * static_cast<std::size_t>(lv_area_get_height(area));
    lv_display_flush_ready(display);
}

void advance(app::MenuApplication& menu,
             const runtime::RenderPolicy& policy,
             unsigned frameCount)
{
    for (unsigned frame = 0; frame < frameCount; ++frame) {
        lv_tick_inc(policy.refreshPeriodMs);
        menu.tick(policy.refreshPeriodMs);
        (void)lv_timer_handler();
    }
}

} // namespace

int main()
{
    const auto policy = runtime::resolveRenderPolicy(
        {
            .onlineCpuCount = 1,
            .totalMemoryKiB = 128U * 1024U,
            .availableMemoryKiB = 64U * 1024U,
        });
    check(policy.profile == runtime::RenderProfile::Low, "test policy is low");
    check(policy.bufferLines == 40, "test uses forty render lines");

    runtime::LvglMemoryPool pool;
    lv_init();
    {
        check(pool.add(static_cast<std::size_t>(policy.extraHeapKiB) * 1024U),
              "add low-profile LVGL pool");

        auto* display = lv_display_create(240, 240);
        check(display != nullptr, "create headless display");
        std::vector<std::uint8_t> drawBuffer(
            240U * static_cast<std::size_t>(policy.bufferLines) * 2U);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_flush_cb(display, flush);
        lv_display_set_buffers(display,
                               drawBuffer.data(),
                               nullptr,
                               drawBuffer.size(),
                               LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_timer_set_period(lv_display_get_refr_timer(display), policy.refreshPeriodMs);

        {
            app::MenuApplication menu;
            check(menu.create(lv_screen_active(), policy), "create low-profile menu");
            check(!menu.usesLargeObjectLayers(), "low profile has no large object layers");
            lv_refr_now(display);
            check(flushCount > 0, "first frame completes");

            menu.handleAction(input::InputAction::Next);
            flushCount = 0;
            flushedPixels = 0;
            advance(menu, policy, 8);
            std::cout << "home-focus: flushes=" << flushCount
                      << " pixels=" << flushedPixels << '\n';
            check(flushedPixels <= 90'000U,
                  "home focus transition stays within the damage budget");

            for (unsigned index = 0; index < 7; ++index) {
                menu.handleAction(input::InputAction::Next);
            }
            check(lv_anim_count_running() <= 1,
                  "rapid navigation keeps only the latest focus animation");
            advance(menu, policy, 8);
            check(lv_anim_count_running() == 0,
                  "retargeted focus animation reaches a stable state");

            // Arc page.
            menu.handleAction(input::InputAction::Activate);
            advance(menu, policy, 10);
            menu.handleAction(input::InputAction::Back);
            advance(menu, policy, 10);

            // Switch page: preserve both interactive toggles.
            menu.handleAction(input::InputAction::Next);
            menu.handleAction(input::InputAction::Activate);
            advance(menu, policy, 10);
            menu.handleAction(input::InputAction::Activate);
            menu.handleAction(input::InputAction::Next);
            menu.handleAction(input::InputAction::Activate);
            advance(menu, policy, 4);
            menu.handleAction(input::InputAction::Back);
            advance(menu, policy, 10);

            // Animation page.
            menu.handleAction(input::InputAction::Next);
            menu.handleAction(input::InputAction::Activate);
            advance(menu, policy, 10);
            menu.handleAction(input::InputAction::Next);
            menu.handleAction(input::InputAction::Activate);
            advance(menu, policy, 4);
            menu.handleAction(input::InputAction::Back);
            advance(menu, policy, 10);

            // Button telemetry page.
            menu.handleAction(input::InputAction::Next);
            menu.handleAction(input::InputAction::Activate);
            advance(menu, policy, 10);
            menu.onButtonTelemetry(
                input::PhysicalButton::Confirm, true, 1, 120, "Press");
            advance(menu, policy, 2);
            menu.handleAction(input::InputAction::Back);
            advance(menu, policy, 10);

            lv_mem_monitor_t memory{};
            lv_mem_monitor(&memory);
            check(memory.free_biggest_size > 0, "LVGL keeps a free contiguous block");
            check(memory.max_used < memory.total_size, "LVGL heap does not exhaust");
        }
        lv_display_delete(display);
    }
    lv_deinit();
    std::cout << "LVGL headless tests passed\n";
}
