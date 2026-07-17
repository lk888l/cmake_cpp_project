add_subdirectory(input)
add_subdirectory(runtime)

if(LVGL_MENU_NEEDS_LVGL)
    add_subdirectory(assets/fonts)
    add_subdirectory(app)
endif()

if(LVGL_MENU_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(LVGL_MENU_BUILD_APP)
    add_subdirectory(bsp)
    add_subdirectory(hardware)
    add_subdirectory(display)
    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        menu_app
        menu_input_linux
        menu_display
        menu_st7789
        menu_bsp
        menu_runtime_core
        menu_lvgl_runtime
        lvgl
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        LVGL_MENU_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
        LVGL_MENU_BASE_HEAP_KIB=${LVGL_MENU_BASE_HEAP_KIB}
    )

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
