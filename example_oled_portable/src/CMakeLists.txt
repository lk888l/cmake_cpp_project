add_library(oled_core STATIC
    bsp/i2c/bsp_i2c.cpp
    display/oled_display.cpp
    display/oled_canvas.cpp
)

target_include_directories(oled_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(oled_core PUBLIC u8g2)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(oled_linux STATIC
        bsp/i2c/linux_i2c_bus.cpp
        app/system_state.cpp
        app/oled_demo_app.cpp
    )
    target_include_directories(oled_linux PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(oled_linux PUBLIC oled_core)

    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE oled_linux)

    if(USE_STATIC_LINKING)
        target_link_options(${PROJECT_NAME} PRIVATE -static -Wl,--gc-sections)
    elseif(NOT MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Release" AND CMAKE_STRIP)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${CMAKE_STRIP}" --strip-unneeded "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Stripping release executable")
    endif()

    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
endif()

if(BUILD_TESTING)
    add_executable(oled_core_tests tests/oled_core_tests.cpp)
    target_link_libraries(oled_core_tests PRIVATE oled_core)
    add_test(NAME oled_core_tests COMMAND oled_core_tests)
endif()
