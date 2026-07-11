add_library(app_lib STATIC lvgl_demo_app.cpp)
target_include_directories(app_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(app_lib PUBLIC display_lvgl_lib lvgl)

