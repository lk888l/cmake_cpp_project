#include "lvgl_demo_app.hpp"

#include "lvgl.h"

namespace app {
namespace {

void buttonClicked(lv_event_t* event)
{
    auto* label = static_cast<lv_obj_t*>(lv_event_get_user_data(event));
    lv_label_set_text(label, "LVGL is running");
}

} // namespace

void createDemoUi()
{
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);

    lv_obj_t* title = lv_label_create(screen);
    lv_label_set_text(title, "LVGL + ST7789");
    lv_obj_set_style_text_color(title, lv_color_hex(0x44D7B6), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    lv_obj_t* status = lv_label_create(screen);
    lv_label_set_text(status, "Linux SPI BSP");
    lv_obj_set_style_text_color(status, lv_color_hex(0xDCE7F2), 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t* button = lv_button_create(screen);
    lv_obj_set_size(button, 150, 44);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, 0, -34);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1677FF), 0);

    lv_obj_t* button_label = lv_label_create(button);
    lv_label_set_text(button_label, "Test");
    lv_obj_center(button_label);
    lv_obj_add_event_cb(button, buttonClicked, LV_EVENT_CLICKED, status);
}

} // namespace app

