add_library(uart_bsp_api INTERFACE)
target_include_directories(uart_bsp_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(uart_app STATIC app/uart_demo_app.cpp)
target_link_libraries(uart_app PUBLIC uart_bsp_api)
target_include_directories(uart_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
configure_project_target(uart_app)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_library(uart_bsp_linux STATIC bsp/uart/linux_uart_port.cpp)
    target_link_libraries(uart_bsp_linux PUBLIC uart_bsp_api)
    target_include_directories(uart_bsp_linux PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    configure_project_target(uart_bsp_linux)

    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE uart_app uart_bsp_linux)
    configure_project_target(${PROJECT_NAME})

    if(USE_STATIC_LINKING)
        target_link_options(${PROJECT_NAME} PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
            -Wl,--gc-sections
        )
    elseif(NOT MSVC)
        target_link_options(${PROJECT_NAME} PRIVATE -Wl,--gc-sections)
    endif()

    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
endif()

if(BUILD_TESTING)
    add_executable(uart_app_tests tests/uart_demo_app_tests.cpp)
    target_link_libraries(uart_app_tests PRIVATE uart_app)
    configure_project_target(uart_app_tests)
    add_test(NAME uart_app_tests COMMAND uart_app_tests)
endif()
