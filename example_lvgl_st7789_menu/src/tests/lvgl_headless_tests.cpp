#include "app/menu_application.hpp"
#include "runtime/lvgl_memory_pool.hpp"
#include "runtime/render_policy.hpp"

#include "lvgl.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

std::size_t flushCount{};

void check(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void flush(lv_display_t* display, const lv_area_t*, std::uint8_t*)
{
    ++flushCount;
    lv_display_flush_ready(display);
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
    check(policy.bufferLines == 8, "test uses eight render lines");

    runtime::LvglMemoryPool pool;
    lv_init();
    {
        check(pool.add(static_cast<std::size_t>(policy.extraHeapKiB) * 1024U),
              "add low-profile LVGL pool");

        auto* display = lv_display_create(240, 240);
        check(display != nullptr, "create headless display");
        std::array<std::uint8_t, 240U * 8U * 2U> drawBuffer{};
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
            menu.handleAction(input::InputAction::Activate);
            for (unsigned index = 0; index < 20; ++index) {
                lv_tick_inc(policy.refreshPeriodMs);
                menu.tick(policy.refreshPeriodMs);
                (void)lv_timer_handler();
            }

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
