add_subdirectory(gpio_button)

if(GPIO_BUTTON_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(GPIO_BUTTON_BUILD_APP)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR
            "The GPIO application requires Linux. Set GPIO_BUTTON_BUILD_APP=OFF for local tests.")
    endif()

    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE gpio_button_linux)

    if(USE_STATIC_LINKING)
        target_link_options(${PROJECT_NAME} PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
            -Wl,--gc-sections
        )
    endif()

    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
endif()
