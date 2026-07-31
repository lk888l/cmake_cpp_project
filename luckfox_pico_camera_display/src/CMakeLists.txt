add_library(camera_display_core
    core/config.cpp
    core/io.cpp
    core/layout.cpp
    core/media.cpp
    core/metrics.cpp
    core/recovery_policy.cpp
    core/rgb565.cpp
    core/status_overlay.cpp
)
target_include_directories(camera_display_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(drivers)

if(CAMERA_DISPLAY_BUILD_APP)
    add_subdirectory(platform)

    add_executable(${PROJECT_NAME}
        app/application.cpp
        app/main.cpp
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        camera_display_core
        camera_display_linux
        camera_display_rockchip
        camera_display_st7789
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        CAMERA_DISPLAY_VERSION="${PROJECT_VERSION}"
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "/oem/usr/lib"
        BUILD_WITH_INSTALL_RPATH ON
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,--as-needed
        -Wl,--no-undefined
        "-Wl,-rpath-link,${LUCKFOX_MEDIA_ROOT}/lib"
    )
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
endif()
