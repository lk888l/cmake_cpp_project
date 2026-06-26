
###########################################
# Toolchain / Target system
###########################################
# 这个文件用于在使用 Buildroot 输出目录时配置交叉编译工具链和 sysroot。
# 主要变量：
# - BUILDROOT_DIR: Buildroot 输出路径（含 host/bin, host/aarch64-buildroot-linux-gnu/sysroot 等）
# - TOOLCHAIN_DIR: 工具链根目录（一般为 ${BUILDROOT_DIR}/host）
# - SYSROOT: 目标根文件系统路径（供链接器查找启动文件和库）

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)     # 目标 CPU：aarch64（RK 系列）

# 用户需设置为 Buildroot 的 output 目录，例：/home/user/buildroot/output
# 这个值可以直接在命令行传入，以支持不同机器或 CI 环境：
#   cmake -S . -B build -DTARGET_PLATFORM=rockchip-arm \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/arm64-buildroot.cmake \
#     -DBUILDROOT_DIR=/home/kk/kk_data/buildroot_p1/buildroot/output
set(BUILDROOT_DIR "/home/kk/kk_data/buildroot_p1/buildroot/output" CACHE PATH "Buildroot output directory")

if(NOT EXISTS "${BUILDROOT_DIR}")
    message(FATAL_ERROR "BUILDROOT_DIR does not exist: ${BUILDROOT_DIR}\nPlease set BUILDROOT_DIR to your Buildroot output directory.")
endif()

set(TOOLCHAIN_DIR ${BUILDROOT_DIR}/host)

# 尝试自动选择合适的 sysroot 路径（按常见位置查找）
set(_SYSROOT_CANDIDATES
    ${BUILDROOT_DIR}/host/aarch64-buildroot-linux-gnu/sysroot
    ${BUILDROOT_DIR}/host/aarch64-linux-gnu/sysroot
    ${BUILDROOT_DIR}/host/sysroot
)
foreach(_p IN LISTS _SYSROOT_CANDIDATES)
    if(EXISTS "${_p}")
        set(SYSROOT ${_p})
        break()
    endif()
endforeach()

if(NOT DEFINED SYSROOT)
    message(FATAL_ERROR "Could not find a suitable sysroot in Buildroot output. Checked: ${_SYSROOT_CANDIDATES}")
endif()

###########################################
# Compiler
###########################################
set(CMAKE_C_COMPILER
    ${TOOLCHAIN_DIR}/bin/aarch64-linux-gnu-gcc)

set(CMAKE_CXX_COMPILER
    ${TOOLCHAIN_DIR}/bin/aarch64-linux-gnu-g++)

set(CMAKE_AR
    ${TOOLCHAIN_DIR}/bin/aarch64-linux-gnu-ar)

set(CMAKE_STRIP
    ${TOOLCHAIN_DIR}/bin/aarch64-linux-gnu-strip)

set(CMAKE_NM
    ${TOOLCHAIN_DIR}/bin/aarch64-linux-gnu-nm)

set(CMAKE_OBJCOPY
    ${TOOLCHAIN_DIR}/bin/aarch64-linux-gnu-objcopy)

###########################################
# Sysroot
###########################################
set(CMAKE_SYSROOT ${SYSROOT})

# 告知 CMake 在查找库/头文件时优先使用 sysroot
set(CMAKE_FIND_ROOT_PATH ${SYSROOT})

###########################################
# Search policy
###########################################
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

###########################################
# pkg-config
###########################################
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${SYSROOT})

# pkg-config 在交叉编译时需要指向 sysroot 下的 pkgconfig 目录（使用 ':' 分隔多个路径）
set(ENV{PKG_CONFIG_LIBDIR} "${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig")

###########################################
# Compile flags
###########################################
## 在 host/bin 下查找常见的交叉工具名称；优先使用 Buildroot 自带的工具链
find_program(TB_AARCH64_GCC
    NAMES aarch64-buildroot-linux-gnu-gcc aarch64-linux-gcc aarch64-linux-gcc-13.3.0
    HINTS ${TOOLCHAIN_DIR}/bin PATHS ENV PATH)

find_program(TB_AARCH64_GXX
    NAMES aarch64-buildroot-linux-gnu-g++ aarch64-linux-g++
    HINTS ${TOOLCHAIN_DIR}/bin PATHS ENV PATH)

find_program(TB_AARCH64_AR
    NAMES aarch64-buildroot-linux-gnu-ar aarch64-linux-ar
    HINTS ${TOOLCHAIN_DIR}/bin PATHS ENV PATH)

find_program(TB_AARCH64_STRIP
    NAMES aarch64-buildroot-linux-gnu-strip aarch64-linux-strip
    HINTS ${TOOLCHAIN_DIR}/bin PATHS ENV PATH)

find_program(TB_AARCH64_NM
    NAMES aarch64-buildroot-linux-gnu-nm aarch64-linux-nm
    HINTS ${TOOLCHAIN_DIR}/bin PATHS ENV PATH)

find_program(TB_AARCH64_OBJCOPY
    NAMES aarch64-buildroot-linux-gnu-objcopy aarch64-linux-objcopy
    HINTS ${TOOLCHAIN_DIR}/bin PATHS ENV PATH)

if(TB_AARCH64_GCC)
    set(CMAKE_C_COMPILER ${TB_AARCH64_GCC})
endif()

if(TB_AARCH64_GXX)
    set(CMAKE_CXX_COMPILER ${TB_AARCH64_GXX})
endif()

if(TB_AARCH64_AR)
    set(CMAKE_AR ${TB_AARCH64_AR})
endif()

if(TB_AARCH64_STRIP)
    set(CMAKE_STRIP ${TB_AARCH64_STRIP})
endif()

if(TB_AARCH64_NM)
    set(CMAKE_NM ${TB_AARCH64_NM})
endif()

if(TB_AARCH64_OBJCOPY)
    set(CMAKE_OBJCOPY ${TB_AARCH64_OBJCOPY})
endif()

message(STATUS "Detected toolchain GCC: ${CMAKE_C_COMPILER}")
message(STATUS "Detected toolchain GXX: ${CMAKE_CXX_COMPILER}")
message(STATUS "Detected toolchain AR: ${CMAKE_AR}")
message(STATUS "Detected toolchain STRIP: ${CMAKE_STRIP}")

# 如果找不到交叉编译器，给出明确错误提示，帮助用户定位问题
if(NOT CMAKE_C_COMPILER OR NOT EXISTS "${CMAKE_C_COMPILER}")
    message(FATAL_ERROR "Cross C compiler not found. Please ensure Buildroot toolchain is present and BUILDROOT_DIR is correct. Looked for: ${TOOLCHAIN_DIR}/bin")
endif()