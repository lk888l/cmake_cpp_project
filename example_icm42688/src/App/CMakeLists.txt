set(TARGET_NAME app_lib)

add_library(${TARGET_NAME} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/gyro_reader_app.cpp"
)

target_include_directories(${TARGET_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${TARGET_NAME} PUBLIC
    hardware_icm42688_lib
    peripheral_lib
    vqf_lib
)
