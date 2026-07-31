set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(LUCKFOX_SDK_ROOT "$ENV{LUCKFOX_SDK_ROOT}" CACHE PATH
    "Luckfox Pico SDK root")
if(NOT LUCKFOX_SDK_ROOT)
    message(FATAL_ERROR
        "LUCKFOX_SDK_ROOT is required. Export it or pass -DLUCKFOX_SDK_ROOT=/path/to/luckfox-pico")
endif()

set(_TOOLCHAIN_ROOT
    "${LUCKFOX_SDK_ROOT}/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf")
file(GLOB _TOOLCHAIN_BIN_CANDIDATES
    "${_TOOLCHAIN_ROOT}/bin"
    "${_TOOLCHAIN_ROOT}/*/bin")
find_program(CMAKE_C_COMPILER
    NAMES arm-rockchip830-linux-uclibcgnueabihf-gcc
    HINTS ${_TOOLCHAIN_BIN_CANDIDATES}
    NO_DEFAULT_PATH REQUIRED)
find_program(CMAKE_CXX_COMPILER
    NAMES arm-rockchip830-linux-uclibcgnueabihf-g++
    HINTS ${_TOOLCHAIN_BIN_CANDIDATES}
    NO_DEFAULT_PATH REQUIRED)
find_program(CMAKE_AR
    NAMES arm-rockchip830-linux-uclibcgnueabihf-ar
    HINTS ${_TOOLCHAIN_BIN_CANDIDATES}
    NO_DEFAULT_PATH REQUIRED)
find_program(CMAKE_RANLIB
    NAMES arm-rockchip830-linux-uclibcgnueabihf-ranlib
    HINTS ${_TOOLCHAIN_BIN_CANDIDATES}
    NO_DEFAULT_PATH)
find_program(CMAKE_STRIP
    NAMES arm-rockchip830-linux-uclibcgnueabihf-strip
    HINTS ${_TOOLCHAIN_BIN_CANDIDATES}
    NO_DEFAULT_PATH)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

