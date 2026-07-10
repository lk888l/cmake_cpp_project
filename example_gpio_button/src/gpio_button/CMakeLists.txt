set(CORE_TARGET gpio_button_core)

add_library(${CORE_TARGET} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/button_state_machine.cpp"
)

target_include_directories(${CORE_TARGET} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

if(GPIO_BUTTON_BUILD_APP)
    set(LINUX_TARGET gpio_button_linux)
    add_library(${LINUX_TARGET} STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/button_manager_linux.cpp"
    )

    target_include_directories(${LINUX_TARGET} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )

    target_link_libraries(${LINUX_TARGET} PUBLIC
        ${CORE_TARGET}
    )
endif()
