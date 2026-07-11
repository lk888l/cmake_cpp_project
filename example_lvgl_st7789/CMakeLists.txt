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

project(example_lvgl_st7789 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE FILEPATH "LVGL configuration" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "Do not build LVGL examples" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "Do not build LVGL demos" FORCE)
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "Do not build ThorVG" FORCE)
set(LVGL_SOURCE_DIR "" CACHE PATH "Existing LVGL source tree; empty downloads the pinned version")
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

message(STATUS "Target platform: ${TARGET_PLATFORM}")
message(STATUS "Static linking: ${USE_STATIC_LINKING}")

add_subdirectory(src)
