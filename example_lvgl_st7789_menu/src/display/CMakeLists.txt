add_library(menu_display STATIC lvgl_st7789_display.cpp)
target_include_directories(menu_display PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(menu_display PUBLIC menu_st7789 lvgl)
