# 告诉 CMake 目标系统是 Linux，架构是 ARM
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)


# 指定 C 和 C++ 交叉编译器
# (前提是它们已经在你的 PATH 环境变量中，否则这里需要写绝对路径)
set(CMAKE_C_COMPILER arm-rockchip830-linux-uclibcgnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-rockchip830-linux-uclibcgnueabihf-g++)

# 指定交叉编译环境的根目录（sysroot），如果你知道具体路径，可以取消注释并修改
# set(CMAKE_SYSROOT /opt/rockchip830/sysroot)

# 调整 CMake 查找库和头文件的行为：
# - 查找程序时，只在宿主机（WSL）查找
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# - 查找库、头文件、包时，只在交叉编译环境中查找（防止误用 WSL x86 的库）
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-a7 -mfloat-abi=hard")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
# set(CMAKE_INSTALL_RPATH "/root/libs")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--dynamic-linker=/lib/ld-uClibc.so.0")