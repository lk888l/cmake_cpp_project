cmake_minimum_required(VERSION 3.10.0)

set(TARGET_PLATFORM "arm-none-linux-gnueabihf" CACHE STRING
    "Select target platform: arm-none-linux-gnueabihf or local")

if(TARGET_PLATFORM STREQUAL "arm-none-linux-gnueabihf")
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_SOURCE_DIR}/cmake/arm-none-linux-gnueabihf.cmake"
        CACHE FILEPATH "Toolchain file")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
    set(_DEFAULT_STATIC_LINKING ON)
elseif(TARGET_PLATFORM STREQUAL "local")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")
    set(_DEFAULT_STATIC_LINKING OFF)
else()
    message(FATAL_ERROR "Unsupported TARGET_PLATFORM: ${TARGET_PLATFORM}")
endif()

if(NOT DEFINED USE_STATIC_LINKING OR USE_STATIC_LINKING STREQUAL "")
    set(USE_STATIC_LINKING ${_DEFAULT_STATIC_LINKING} CACHE BOOL "Use static linking")
else()
    set(USE_STATIC_LINKING ${USE_STATIC_LINKING} CACHE BOOL "Use static linking")
endif()

project(example_gpio_button VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(GPIO_BUTTON_BUILD_APP "Build the Linux GPIO v2 button demonstration" ON)
option(GPIO_BUTTON_BUILD_TESTS "Build state-machine tests" ON)

if(GPIO_BUTTON_BUILD_TESTS)
    include(CTest)
endif()

message(STATUS "Target platform: ${TARGET_PLATFORM}")
message(STATUS "Static linking: ${USE_STATIC_LINKING}")

add_subdirectory(src)
