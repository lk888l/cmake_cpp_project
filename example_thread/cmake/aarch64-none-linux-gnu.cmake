#############################################
# CMake Toolchain File for aarch64-none-linux-gnu
#############################################
# 
# 用于 aarch64-none-linux-gnu 工具链的交叉编译配置
# 遵循 CMake 官方最佳实践：
#   - https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html
#   - https://cmake.org/cmake/help/latest/manual/cmake-compile-features.7.html
#
# 使用方法：
#   cmake -S . -B build \
#     -DTARGET_PLATFORM=aarch64-gnu \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-none-linux-gnu.cmake \
#     [-DSYSROOT_PATH=/path/to/sysroot] \
#     [-DTOOLCHAIN_PATH=/path/to/toolchain/bin]
#
# 参数说明：
#   - SYSROOT_PATH: 目标系统根文件系统路径（可选，若不指定则尝试自动查找）
#   - TOOLCHAIN_PATH: 工具链二进制文件所在目录（默认从 PATH 搜索）

cmake_minimum_required(VERSION 3.20)

#############################################
# 1. 基本系统和处理器配置
#############################################
set(CMAKE_SYSTEM_NAME             Linux)
set(CMAKE_SYSTEM_PROCESSOR        aarch64)
set(CMAKE_SYSTEM_VERSION          1)

# 禁用系统检查以加速 CMake 配置
set(CMAKE_SYSTEM_IMPLEMENTATION   LINUX)
set(CMAKE_SYSTEM_IMPLEMENTATION_LOWER linux)

#############################################
# 2. 工具链和 Sysroot 配置
#############################################

# 工具链二进制文件目录（用户可通过 -DTOOLCHAIN_PATH 覆盖）
set(TOOLCHAIN_PATH "" CACHE PATH 
    "Path to aarch64-none-linux-gnu toolchain bin directory (leave empty to search in PATH)")

# Sysroot 路径（用户可通过 -DSYSROOT_PATH 覆盖）
set(SYSROOT_PATH "" CACHE PATH 
    "Path to target sysroot directory (leave empty to disable)")

# 如果指定了 SYSROOT_PATH，验证其存在
if(DEFINED SYSROOT_PATH AND NOT SYSROOT_PATH STREQUAL "")
    if(NOT EXISTS "${SYSROOT_PATH}")
        message(FATAL_ERROR "SYSROOT_PATH does not exist: ${SYSROOT_PATH}")
    endif()
    set(CMAKE_SYSROOT ${SYSROOT_PATH})
    message(STATUS "[Toolchain] Using sysroot: ${CMAKE_SYSROOT}")
endif()

#############################################
# 3. 编译器和工具设置
#############################################

# 编译器前缀
set(_COMPILER_PREFIX "aarch64-none-linux-gnu")

# 如果指定了 TOOLCHAIN_PATH，从该目录查找工具
if(TOOLCHAIN_PATH)
    # 直接使用指定路径
    set(CMAKE_C_COMPILER   "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-gcc")
    set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-g++")
    set(CMAKE_AR           "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-ar")
    set(CMAKE_RANLIB       "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-ranlib")
    set(CMAKE_STRIP        "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-strip")
    set(CMAKE_OBJCOPY      "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-objcopy")
    set(CMAKE_NM           "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-nm")
    set(CMAKE_OBJDUMP      "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-objdump")
    set(CMAKE_ADDR2LINE    "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-addr2line")
    set(CMAKE_GDB          "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-gdb")
else()
    # 从 PATH 中自动查找工具
    find_program(_C_COMPILER NAMES ${_COMPILER_PREFIX}-gcc REQUIRED)
    find_program(_CXX_COMPILER NAMES ${_COMPILER_PREFIX}-g++ REQUIRED)
    find_program(_AR NAMES ${_COMPILER_PREFIX}-ar REQUIRED)
    find_program(_RANLIB NAMES ${_COMPILER_PREFIX}-ranlib)
    find_program(_STRIP NAMES ${_COMPILER_PREFIX}-strip)
    find_program(_OBJCOPY NAMES ${_COMPILER_PREFIX}-objcopy)
    find_program(_NM NAMES ${_COMPILER_PREFIX}-nm)
    find_program(_OBJDUMP NAMES ${_COMPILER_PREFIX}-objdump)
    find_program(_ADDR2LINE NAMES ${_COMPILER_PREFIX}-addr2line)
    find_program(_GDB NAMES ${_COMPILER_PREFIX}-gdb)

    set(CMAKE_C_COMPILER   ${_C_COMPILER})
    set(CMAKE_CXX_COMPILER ${_CXX_COMPILER})
    set(CMAKE_AR           ${_AR})
    if(_RANLIB)
        set(CMAKE_RANLIB   ${_RANLIB})
    endif()
    if(_STRIP)
        set(CMAKE_STRIP    ${_STRIP})
    endif()
    if(_OBJCOPY)
        set(CMAKE_OBJCOPY  ${_OBJCOPY})
    endif()
    if(_NM)
        set(CMAKE_NM       ${_NM})
    endif()
    if(_OBJDUMP)
        set(CMAKE_OBJDUMP  ${_OBJDUMP})
    endif()
    if(_ADDR2LINE)
        set(CMAKE_ADDR2LINE ${_ADDR2LINE})
    endif()
    if(_GDB)
        set(CMAKE_GDB      ${_GDB})
    endif()
endif()

#############################################
# 4. CMake 搜索路径配置
#############################################

# 设置 CMake 在查找程序、库、头文件时的搜索根路径
if(CMAKE_SYSROOT)
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
    
    # 官方推荐的搜索策略：
    # NEVER：在主机系统中查找（用于工具程序）
    # ONLY：仅在 sysroot 中查找（用于库和头文件）
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

#############################################
# 5. 编译和链接标志
#############################################

# 基础编译标志（可以在主 CMakeLists.txt 中进一步调整）
set(CMAKE_C_FLAGS_INIT "-march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a")

# 如果使用 sysroot，添加相应的编译标志
if(CMAKE_SYSROOT)
    string(APPEND CMAKE_C_FLAGS_INIT " --sysroot=${CMAKE_SYSROOT}")
    string(APPEND CMAKE_CXX_FLAGS_INIT " --sysroot=${CMAKE_SYSROOT}")
endif()

# Release 版本优化标志
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "C release flags")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "C++ release flags")

# Debug 版本调试标志
set(CMAKE_C_FLAGS_DEBUG "-g -O0" CACHE STRING "C debug flags")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0" CACHE STRING "C++ debug flags")

#############################################
# 6. 链接器配置
#############################################

# 链接器标志
if(CMAKE_SYSROOT)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "--sysroot=${CMAKE_SYSROOT}")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "--sysroot=${CMAKE_SYSROOT}")
    set(CMAKE_MODULE_LINKER_FLAGS_INIT "--sysroot=${CMAKE_SYSROOT}")
endif()

# 跳过链接器检查（在交叉编译时通常需要）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

#############################################
# 7. pkg-config 支持
#############################################

if(CMAKE_SYSROOT)
    set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
    set(ENV{PKG_CONFIG_LIBDIR} 
        "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig:${CMAKE_SYSROOT}/lib/pkgconfig")
endif()

#############################################
# 8. 诊断信息
#############################################

message(STATUS "========== Toolchain Configuration ==========")
message(STATUS "System:              ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "C Compiler:          ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler:        ${CMAKE_CXX_COMPILER}")
message(STATUS "AR:                  ${CMAKE_AR}")
message(STATUS "Sysroot:             ${CMAKE_SYSROOT}")
if(TOOLCHAIN_PATH)
    message(STATUS "Toolchain Path:      ${TOOLCHAIN_PATH}")
else()
    message(STATUS "Toolchain Path:      (auto-detected from PATH)")
endif()
message(STATUS "===========================================")

#############################################
# 9. 编译器验证（可选）
#############################################

# 如果需要验证编译器能否工作，可以启用此代码
# include(CMakeTestCCompiler)
# include(CMakeTestCXXCompiler)
