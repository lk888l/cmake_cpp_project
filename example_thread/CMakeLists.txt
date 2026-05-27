cmake_minimum_required(VERSION 3.10.0)

# 项目说明：
# - 支持两种构建目标：`rockchip-arm`（交叉编译，使用 Buildroot 输出的工具链）
#   与 `local-x86`（本地 x86 构建，使用系统编译器）。
# - 通过设置 `-DTARGET_PLATFORM=` 选择目标平台；默认值为 `rockchip-arm`（可以改为 local-x86）。
# 示例：
#   本地构建（快速测试）：
#     cmake -S . -B build -DTARGET_PLATFORM=local-x86
#     cmake --build build -- -j
#   交叉构建（使用 Buildroot 输出的 toolchain）：
#     cmake -S . -B build -DTARGET_PLATFORM=rockchip-arm \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/arm64-buildroot.cmake \
#       -DBUILDROOT_DIR=/path/to/buildroot/output
#
# 注意：`BUILDROOT_DIR` 可在 toolchain 文件中默认设置，也可直接通过 -D 参数覆盖。

# 设置构建目标平台，默认为 arm-linux（交叉编译 Buildroot）。
# 支持的平台：
#   - arm-linux          : Buildroot aarch64-buildroot-linux-gnu
#   - aarch64-gnu        : 独立的 aarch64-none-linux-gnu 工具链
#   - local              : 本地开发
set(TARGET_PLATFORM "aarch64-gnu" CACHE STRING 
    "Select target platform: arm-linux, aarch64-gnu, or local")

# ====== 平台配置 ======
# 根据目标平台调整：toolchain 文件、默认构建类型
if(TARGET_PLATFORM STREQUAL "arm-linux")
    # 使用 Buildroot 预定义的 toolchain 文件（可以通过 -DCMAKE_TOOLCHAIN_FILE 覆盖）
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/arm64-buildroot.cmake" CACHE FILEPATH "Toolchain file")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
    set(_DEFAULT_STATIC_LINKING OFF)

elseif(TARGET_PLATFORM STREQUAL "aarch64-gnu")
    # 使用 aarch64-none-linux-gnu 工具链（标准 GNU 工具链）
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/aarch64-none-linux-gnu.cmake" CACHE FILEPATH "Toolchain file")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
    set(_DEFAULT_STATIC_LINKING ON)

elseif(TARGET_PLATFORM STREQUAL "local")
    # 本地构建：使用宿主机编译器，便于快速迭代与调试
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")
    set(_DEFAULT_STATIC_LINKING OFF)
endif()

if(NOT DEFINED USE_STATIC_LINKING OR USE_STATIC_LINKING STREQUAL "")
    set(USE_STATIC_LINKING ${_DEFAULT_STATIC_LINKING} CACHE BOOL 
        "Enable static linking (default: ON for cross-compile, OFF for local build)")
else()
    set(USE_STATIC_LINKING ${USE_STATIC_LINKING} CACHE BOOL 
        "Enable static linking (default: ON for cross-compile, OFF for local build)")
endif()

message(STATUS "Static linking: ${USE_STATIC_LINKING} (platform: ${TARGET_PLATFORM})")

project(example_thread VERSION 1.0.0 LANGUAGES C CXX)

# 导出编译命令文件（供 clangd / 其他工具使用）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译语言标准要求
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 子目录：项目源码
add_subdirectory(src)