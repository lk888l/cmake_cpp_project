cmake_minimum_required(VERSION 3.10.0)


# 设置构建目标平台，默认 rockchip-arm
set(TARGET_PLATFORM "rockchip-arm" CACHE STRING "Select target platform: local-x86 or rockchip-arm")

if(TARGET_PLATFORM STREQUAL "rockchip-arm")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/arm-rockchip830.cmake" CACHE FILEPATH "Toolchain file")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
    set(USE_STATIC_LINKING ON CACHE BOOL "Use static linking")
else()
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")
    set(USE_STATIC_LINKING OFF CACHE BOOL "Use static linking")
endif()


project(example_uart VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译标准设置
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    # 开启全警告、额外警告，并将警告视为错误以保证代码合规
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    # 禁用 RTTI 以减少二进制大小和潜在的安全风险，适用于不使用动态类型识别的项目
    # add_compile_options(-fno-rtti)
endif()

# 业务子目录
add_subdirectory(src)