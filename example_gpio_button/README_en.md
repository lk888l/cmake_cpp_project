# C++23 Linux GPIO Button

[中文](README_cn.md) | **English**

See the [detailed module guide](doc/gpio-button-module_en.md).

Standalone GPIO-button example for embedded Linux. It uses Linux GPIO
character-device v2 ioctls directly and waits for GPIO, `timerfd`, and the stop
`eventfd` in one `epoll` loop. It has no GPIO polling and no libgpiod or
pkg-config dependency.

Defaults are 25 ms debounce, 600 ms long press, and a 250 ms double-click
window. A short release immediately produces `Click`; the second valid click
also produces `DoubleClick`; a release after `LongPress` does not click.

## Structure

```text
example_gpio_button/
├── cmake/arm-none-linux-gnueabihf.cmake
├── src/
│   ├── gpio_button/             # API, state machine, Linux manager
│   ├── tests/                   # Hardware-independent tests
│   ├── CMakeLists.txt
│   └── main.cpp
├── CMakeLists.txt
└── README.md
```

## Cross-build

```powershell
cmake -S example_gpio_button -B build-gpio-arm -G "MinGW Makefiles" `
  -DTOOLCHAIN_PATH=C:/toolchains/arm-none-linux-gnueabihf/bin `
  -DSYSROOT_PATH=C:/target-rootfs
cmake --build build-gpio-arm --parallel
```

With no `TOOLCHAIN_PATH`, CMake searches `PATH` and
`C:/kk_software/toolchain`. The sysroot headers and target kernel must support
`GPIO_V2_GET_LINE_IOCTL`.

## Host state-machine tests

```powershell
cmake -S example_gpio_button -B build-gpio-tests -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=local `
  -DGPIO_BUTTON_BUILD_APP=OFF
cmake --build build-gpio-tests --parallel
ctest --test-dir build-gpio-tests --output-on-failure
```

## Board integration

Set `chip_path`, `line_offset`, and `active_low` in `src/main.cpp`. Configure
pinmux and bias in the device tree. Do not bind the same line to `gpio-keys`
while userspace owns it. Callbacks run on one event thread; forward expensive
work to an application worker.
