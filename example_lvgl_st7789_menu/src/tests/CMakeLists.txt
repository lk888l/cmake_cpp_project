if(BUILD_TESTING)
    add_executable(button_state_machine_tests button_state_machine_tests.cpp)
    target_link_libraries(button_state_machine_tests PRIVATE menu_input_core)
    add_test(NAME button_state_machine_tests COMMAND button_state_machine_tests)

    add_executable(three_key_lvgl_input_tests three_key_lvgl_input_tests.cpp)
    target_link_libraries(three_key_lvgl_input_tests PRIVATE menu_input_core)
    add_test(NAME three_key_lvgl_input_tests COMMAND three_key_lvgl_input_tests)

    add_executable(menu_model_tests menu_model_tests.cpp)
    target_link_libraries(menu_model_tests PRIVATE menu_input_core)
    target_include_directories(menu_model_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
    add_test(NAME menu_model_tests COMMAND menu_model_tests)

    add_executable(render_policy_tests render_policy_tests.cpp)
    target_link_libraries(render_policy_tests PRIVATE menu_runtime_core)
    add_test(NAME render_policy_tests COMMAND render_policy_tests)

    if(LVGL_MENU_BUILD_HEADLESS_TESTS)
        add_executable(lvgl_headless_tests lvgl_headless_tests.cpp)
        target_link_libraries(lvgl_headless_tests PRIVATE
            menu_app
            menu_lvgl_runtime
            lvgl
        )
        add_test(NAME lvgl_headless_tests COMMAND lvgl_headless_tests)
    endif()
endif()
