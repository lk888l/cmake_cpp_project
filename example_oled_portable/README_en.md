# Portable OLED display example

[中文](README_cn.md) | **English**

Lightweight OLED graphics project for Linux/Buildroot, ESP32, and STM32. The
current BSP runs on Linux and targets 0.96-inch 128x64 SSD1306/SSD1315 I2C
panels.

## Documentation

- [Documentation center](docs/README_en.md)
- [Architecture and flow](docs/architecture_en.md)
- [Public API](docs/api-reference_en.md)
- [Grayscale and rendering](docs/grayscale-and-rendering_en.md)
- [Linux / Buildroot](docs/linux-buildroot_en.md)
- [STM32F4 / ESP32 porting](docs/mcu-porting_en.md)
- [Testing and troubleshooting](docs/testing-and-troubleshooting_en.md)

## Design

- C++17 core with application, display, and BSP boundaries.
- Vendored U8g2 C sources; target builds need no network.
- One 1024-byte monochrome framebuffer for a 128x64 panel.
- Linux I2C backend plus portable canvas/display interfaces.
- Spatial Bayer dithering for four apparent intensity levels.

## Layout

```text
src/app/                 # Demo pages and CLI
src/display/             # OledDisplay, OledCanvas, U8g2 bridge
src/bsp/i2c/             # Portable I2C interface and Linux implementation
src/tests/               # Host tests
third_party/u8g2/        # Pinned upstream U8g2 source
cmake/                   # Toolchain files
```

## ARM Linux build

```powershell
cmake -S . -B build-arm -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf
cmake --build build-arm --parallel
```

Use `TOOLCHAIN_PATH` and `SYSROOT_PATH` when automatic discovery is not enough.

## Host core tests

```powershell
cmake -S . -B build-tests -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=local -DOLED_BUILD_APP=OFF
cmake --build build-tests --parallel
ctest --test-dir build-tests --output-on-failure
```

## Board execution

```sh
./example_oled_portable --bus /dev/i2c-3 --address 0x3c
./example_oled_portable --demo graphics
./example_oled_portable --demo grayscale --once
```

Confirm the actual I2C bus/address first. MCU support reuses the core but
requires a platform I2C BSP, delay hook, build integration, and resource review.

## Subsequent MCU port

Reuse `src/display` and the `bsp::I2CDevice` contract. Replace the Linux BSP,
system-state collector, and `main.cpp` with the target HAL/RTOS integration.
Review the framebuffer, text scratch buffer, task stack, font set, and math
library in the final linker map. See
[STM32F4 / ESP32 porting](docs/mcu-porting_en.md) for the complete procedure.
