set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_SYSTEM_VERSION 1)

set(TOOLCHAIN_PATH "" CACHE PATH
    "Path to the arm-none-linux-gnueabihf toolchain bin directory")
set(SYSROOT_PATH "" CACHE PATH
    "Path to the target sysroot directory; leave empty for the toolchain default")

if(SYSROOT_PATH)
    if(NOT EXISTS "${SYSROOT_PATH}")
        message(FATAL_ERROR "SYSROOT_PATH does not exist: ${SYSROOT_PATH}")
    endif()
    set(CMAKE_SYSROOT "${SYSROOT_PATH}")
endif()

set(_COMPILER_PREFIX "arm-none-linux-gnueabihf")

if(TOOLCHAIN_PATH)
    set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-gcc")
    set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-g++")
    set(CMAKE_AR "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-ar")
    set(CMAKE_RANLIB "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-ranlib")
    set(CMAKE_STRIP "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-strip")
    set(CMAKE_READELF "${TOOLCHAIN_PATH}/${_COMPILER_PREFIX}-readelf")
else()
    file(GLOB _LOCAL_TOOLCHAIN_BIN_CANDIDATES
        "C:/kk_software/toolchain/*${_COMPILER_PREFIX}*/bin")

    find_program(_CXX_COMPILER NAMES ${_COMPILER_PREFIX}-g++
        HINTS ${_LOCAL_TOOLCHAIN_BIN_CANDIDATES} PATHS ENV PATH REQUIRED)
    find_program(_AR NAMES ${_COMPILER_PREFIX}-ar
        HINTS ${_LOCAL_TOOLCHAIN_BIN_CANDIDATES} PATHS ENV PATH REQUIRED)
    find_program(_RANLIB NAMES ${_COMPILER_PREFIX}-ranlib
        HINTS ${_LOCAL_TOOLCHAIN_BIN_CANDIDATES} PATHS ENV PATH)
    find_program(_STRIP NAMES ${_COMPILER_PREFIX}-strip
        HINTS ${_LOCAL_TOOLCHAIN_BIN_CANDIDATES} PATHS ENV PATH)
    find_program(_READELF NAMES ${_COMPILER_PREFIX}-readelf
        HINTS ${_LOCAL_TOOLCHAIN_BIN_CANDIDATES} PATHS ENV PATH)

    set(CMAKE_CXX_COMPILER "${_CXX_COMPILER}")
    set(CMAKE_AR "${_AR}")
    if(_RANLIB)
        set(CMAKE_RANLIB "${_RANLIB}")
    endif()
    if(_STRIP)
        set(CMAKE_STRIP "${_STRIP}")
    endif()
    if(_READELF)
        set(CMAKE_READELF "${_READELF}")
    endif()
endif()

if(CMAKE_SYSROOT)
    set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

message(STATUS "========== Toolchain Configuration ==========")
message(STATUS "System:       ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "AR:           ${CMAKE_AR}")
message(STATUS "Sysroot:      ${CMAKE_SYSROOT}")
message(STATUS "============================================")
