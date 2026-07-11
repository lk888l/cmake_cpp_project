add_library(peripheral_lib STATIC
    bsp_status.cpp
    gpio/bsp_gpio.cpp
    gpio/linux_gpio.cpp
    spi/linux_spi_bus.cpp
)
target_include_directories(peripheral_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

