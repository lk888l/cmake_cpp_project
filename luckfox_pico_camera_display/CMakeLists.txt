cmake_minimum_required(VERSION 3.16)

project(luckfox_pico_camera_display VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CAMERA_DISPLAY_BUILD_APP "Build the RV1103 camera display application" OFF)
option(CAMERA_DISPLAY_BUILD_TESTS "Build host-testable core tests" ON)
option(CAMERA_DISPLAY_ENABLE_SANITIZERS "Enable host ASan/UBSan" OFF)
set(LUCKFOX_SDK_ROOT "" CACHE PATH "Luckfox Pico SDK root")
set(LUCKFOX_MEDIA_ROOT "" CACHE PATH
    "Rockchip media output root containing include/ and lib/; defaults to SDK output")

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wformat=2
    )
endif()

if(CAMERA_DISPLAY_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "CAMERA_DISPLAY_ENABLE_SANITIZERS is supported with GCC/Clang only")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(CAMERA_DISPLAY_BUILD_TESTS)
    include(CTest)
endif()

if(CAMERA_DISPLAY_BUILD_APP)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "The hardware application can only target Linux")
    endif()
    if(NOT LUCKFOX_SDK_ROOT)
        message(FATAL_ERROR "Set LUCKFOX_SDK_ROOT to the official Luckfox SDK")
    endif()
    if(NOT LUCKFOX_MEDIA_ROOT)
        set(LUCKFOX_MEDIA_ROOT "${LUCKFOX_SDK_ROOT}/output/out/media_out")
    endif()
endif()

add_subdirectory(src)

if(CAMERA_DISPLAY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
