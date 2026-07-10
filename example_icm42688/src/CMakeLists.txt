add_subdirectory(Peripheral)
add_subdirectory(HardWare)
add_subdirectory(third_party/vqf)
add_subdirectory(App)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
    app_lib
    hardware_icm42688_lib
    peripheral_lib
    vqf_lib
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
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
