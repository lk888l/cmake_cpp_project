add_subdirectory(Peripheral)
add_subdirectory(HardWare)
add_subdirectory(Display)
add_subdirectory(App)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE
    app_lib
    display_lvgl_lib
    hardware_st7789_lib
    peripheral_lib
    lvgl
    m
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

