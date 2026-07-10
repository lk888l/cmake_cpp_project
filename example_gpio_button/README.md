# C++23 Linux GPIO Button

详细模块说明见 [`doc/gpio-button-module.md`](doc/gpio-button-module.md)。

独立的 Linux 嵌入式 GPIO 按键示例工程。工程结构直接参考 `example_icm42688`：顶层负责平台与工具链选择，`src` 负责组装目标，各功能子目录维护自己的静态库。

运行时直接使用 Linux GPIO character-device v2 ioctl，并通过 `epoll` 统一等待 GPIO、`timerfd` 和停止用 `eventfd`，没有 GPIO 轮询，也不依赖 libgpiod 或 pkg-config。

默认消抖 25 ms、长按 600 ms、双击窗口 250 ms。短按释放立即产生 `Click`；第二次有效短按额外产生 `DoubleClick`；触发长按后释放不再产生 `Click`。

## 工程结构

```text
example_gpio_button/
├── cmake/
│   └── arm-none-linux-gnueabihf.cmake
├── src/
│   ├── gpio_button/
│   │   ├── CMakeLists.txt
│   │   ├── button.hpp
│   │   ├── button_state_machine.cpp
│   │   └── button_manager_linux.cpp
│   ├── tests/
│   │   ├── CMakeLists.txt
│   │   └── button_state_machine_tests.cpp
│   ├── CMakeLists.txt
│   └── main.cpp
├── CMakeLists.txt
└── README.md
```

## 交叉编译

工程默认选择 `arm-none-linux-gnueabihf`，并使用工程内部的 `cmake/arm-none-linux-gnueabihf.cmake`：

```powershell
cmake -S example_gpio_button -B build-gpio-arm -G "MinGW Makefiles" `
  -DTOOLCHAIN_PATH=C:/toolchains/arm-none-linux-gnueabihf/bin `
  -DSYSROOT_PATH=C:/target-rootfs
cmake --build build-gpio-arm --parallel
```

`TOOLCHAIN_PATH` 留空时会从 `PATH` 以及 `C:/kk_software/toolchain` 自动查找编译器。工具链 sysroot 需要提供包含 GPIO character-device v2 定义的 `linux/gpio.h`，目标板内核也必须支持 `GPIO_V2_GET_LINE_IOCTL`。

## 主机状态机测试

主机测试不编译 Linux GPIO 后端，也不需要 GPIO 硬件：

```powershell
cmake -S example_gpio_button -B build-gpio-tests -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=local `
  -DGPIO_BUTTON_BUILD_APP=OFF
cmake --build build-gpio-tests --parallel
ctest --test-dir build-gpio-tests --output-on-failure
```

## 板级接入

在 `src/main.cpp` 设置 `chip_path`、`line_offset` 和 `active_low`。设备树需正确配置 pinmux 与上拉/下拉；用户态程序独占该 GPIO 时，不要同时绑定内核 `gpio-keys`。回调运行在单一事件线程，耗时工作应转交应用层工作线程。
