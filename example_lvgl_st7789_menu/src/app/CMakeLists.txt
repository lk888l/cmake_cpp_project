add_library(menu_app STATIC menu_application.cpp)
target_include_directories(menu_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(menu_app PUBLIC menu_input_core menu_runtime_core menu_ui_fonts lvgl)
