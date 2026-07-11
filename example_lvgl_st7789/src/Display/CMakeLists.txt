add_library(display_lvgl_lib STATIC lvgl_st7789_display.cpp)
target_include_directories(display_lvgl_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(display_lvgl_lib PUBLIC hardware_st7789_lib lvgl)

