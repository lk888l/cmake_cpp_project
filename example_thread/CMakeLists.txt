cmake_minimum_required(VERSION 3.10.0)


# 设置构建目标平台，默认为 local-x86
# set(TARGET_PLATFORM "rockchip-arm" CACHE STRING "Select target platform: rockchip-arm or local-x86")

# 根据平台选择配置
# if(TARGET_PLATFORM STREQUAL "rockchip-arm")
    # 对应 JSON 中的 rockchip-arm 预设
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/cmake/arm-rockchip830.cmake" CACHE FILEPATH "Toolchain file")
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type")
set(USE_STATIC_LINKING ON CACHE BOOL "Use static linking")
    
# elseif(TARGET_PLATFORM STREQUAL "local-x86")
#     # 对应 JSON 中的 local-x86 预设
#     set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type")
#     set(USE_STATIC_LINKING OFF CACHE BOOL "Use static linking")
# endif()


project(example_thread VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 编译标准设置[
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 业务子目录
add_subdirectory(src)