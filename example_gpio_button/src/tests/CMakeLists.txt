if(BUILD_TESTING)
    add_executable(button_state_machine_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/button_state_machine_tests.cpp"
    )

    target_include_directories(button_state_machine_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../gpio_button"
    )

    target_link_libraries(button_state_machine_tests PRIVATE gpio_button_core)
    add_test(NAME button_state_machine_tests COMMAND button_state_machine_tests)
endif()
