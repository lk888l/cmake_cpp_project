add_library(menu_ui_fonts STATIC
    lv_font_ui_cn_16.c
    lv_font_ui_cn_20.c
)
target_include_directories(menu_ui_fonts PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(menu_ui_fonts PUBLIC lvgl)
target_compile_definitions(menu_ui_fonts PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
