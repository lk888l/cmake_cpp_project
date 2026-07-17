add_library(menu_input_core STATIC
    button_state_machine.cpp
    three_key_lvgl_input.cpp
)
target_include_directories(menu_input_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(LVGL_MENU_BUILD_APP)
    find_package(Threads REQUIRED)
    add_library(menu_input_linux STATIC button_manager_linux.cpp)
    target_include_directories(menu_input_linux PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_link_libraries(menu_input_linux PUBLIC menu_input_core Threads::Threads)
endif()
