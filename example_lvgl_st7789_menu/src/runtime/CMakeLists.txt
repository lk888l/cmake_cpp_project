add_library(menu_runtime_core STATIC render_policy.cpp)
target_include_directories(menu_runtime_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(LVGL_MENU_NEEDS_LVGL)
    add_library(menu_lvgl_runtime STATIC lvgl_memory_pool.cpp)
    target_include_directories(menu_lvgl_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(menu_lvgl_runtime PUBLIC lvgl)
endif()
