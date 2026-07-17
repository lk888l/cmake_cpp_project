add_library(menu_bsp STATIC
    bsp_status.cpp
    gpio/linux_gpio_output.cpp
    spi/linux_spi_bus.cpp
)
target_include_directories(menu_bsp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
