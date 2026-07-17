cmake_minimum_required(VERSION 3.20)

set(TARGET_PLATFORM "arm-none-linux-gnueabihf" CACHE STRING
    "Select target platform: arm-none-linux-gnueabihf or local")
set_property(CACHE TARGET_PLATFORM PROPERTY STRINGS arm-none-linux-gnueabihf local)

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

project(example_uart VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

function(configure_project_target target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    if(TARGET_PLATFORM STREQUAL "arm-none-linux-gnueabihf")
        target_compile_options(${target_name} PRIVATE -mcpu=cortex-a7 -mfloat-abi=hard)
    endif()
endfunction()

message(STATUS "Target platform: ${TARGET_PLATFORM}")
message(STATUS "Static linking: ${USE_STATIC_LINKING}")

add_subdirectory(src)
