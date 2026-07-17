cmake_minimum_required(VERSION 3.16)

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
endif()

project(example_lvgl_st7789_menu VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(LVGL_MENU_BUILD_APP "Build the Linux ST7789 menu application" ON)
option(LVGL_MENU_BUILD_TESTS "Build host-testable core tests" ON)
option(LVGL_MENU_BUILD_HEADLESS_TESTS "Build optional LVGL headless render tests" OFF)
set(LVGL_MENU_BASE_HEAP_KIB "64" CACHE STRING
    "LVGL built-in TLSF base heap size in KiB")
set(LVGL_SOURCE_DIR "" CACHE PATH
    "Existing LVGL source tree; empty downloads the pinned version when the app is enabled")

if(NOT LVGL_MENU_BASE_HEAP_KIB MATCHES "^[0-9]+$" OR
   LVGL_MENU_BASE_HEAP_KIB LESS 16)
    message(FATAL_ERROR "LVGL_MENU_BASE_HEAP_KIB must be an integer of at least 16")
endif()
math(EXPR LVGL_MENU_BASE_HEAP_BYTES "${LVGL_MENU_BASE_HEAP_KIB} * 1024")
set(LVGL_MENU_NEEDS_LVGL OFF)
if(LVGL_MENU_BUILD_APP OR LVGL_MENU_BUILD_HEADLESS_TESTS)
    set(LVGL_MENU_NEEDS_LVGL ON)
endif()

if(LVGL_MENU_BUILD_TESTS)
    include(CTest)
endif()

if(LVGL_MENU_BUILD_APP)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR
            "The application requires Linux. Set LVGL_MENU_BUILD_APP=OFF for local tests.")
    endif()

endif()

if(TARGET_PLATFORM STREQUAL "arm-none-linux-gnueabihf" AND
   NOT CMAKE_BUILD_TYPE STREQUAL "Release" AND
   NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    message(WARNING
        "ARM display build is '${CMAKE_BUILD_TYPE}'. Use the arm-release preset; "
        "unoptimized LVGL software rendering can saturate small SoCs.")
endif()

if(LVGL_MENU_NEEDS_LVGL)
    include(FetchContent)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
    configure_file(lv_conf.h.in "${CMAKE_BINARY_DIR}/generated/lv_conf.h" @ONLY)
    set(LV_CONF_PATH "${CMAKE_BINARY_DIR}/generated/lv_conf.h"
        CACHE FILEPATH "LVGL configuration" FORCE)
    set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "Do not build LVGL examples" FORCE)
    set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "Do not build LVGL demos" FORCE)
    set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "Do not build ThorVG" FORCE)
    if(LVGL_SOURCE_DIR)
        if(NOT EXISTS "${LVGL_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR "LVGL_SOURCE_DIR is not an LVGL source tree: ${LVGL_SOURCE_DIR}")
        endif()
        add_subdirectory("${LVGL_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/lvgl")
    else()
        FetchContent_Declare(lvgl
            GIT_REPOSITORY https://github.com/lvgl/lvgl.git
            GIT_TAG v9.2.2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(lvgl)
    endif()
endif()

message(STATUS "Target platform: ${TARGET_PLATFORM}")
message(STATUS "Build application: ${LVGL_MENU_BUILD_APP}")
message(STATUS "Build tests: ${LVGL_MENU_BUILD_TESTS}")
message(STATUS "Build headless LVGL tests: ${LVGL_MENU_BUILD_HEADLESS_TESTS}")
message(STATUS "LVGL base heap: ${LVGL_MENU_BASE_HEAP_KIB} KiB")
message(STATUS "Static linking: ${USE_STATIC_LINKING}")

add_subdirectory(src)
