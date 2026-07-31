# OLED driver documentation center

[中文](README_cn.md) | **English**

These documents describe the current `example_oled_portable` implementation,
usage, and porting boundaries. Except for explicitly identified porting advice,
the content is derived from the source. Linux builds and core unit tests have
been validated; physical OLED and MCU behavior still require their
corresponding hardware.

## Reading path

| Document | Content | Audience |
|---|---|---|
| [architecture_en.md](architecture_en.md) | Layers, ownership, initialization, refresh | Architecture review |
| [api-reference_en.md](api-reference_en.md) | Display, canvas, and I2C API | Page developers |
| [grayscale-and-rendering_en.md](grayscale-and-rendering_en.md) | Framebuffer, coordinates, fonts, dithering | Graphics/performance |
| [linux-buildroot_en.md](linux-buildroot_en.md) | Build, deploy, wiring, CLI | Linux integration |
| [mcu-porting_en.md](mcu-porting_en.md) | STM32F4 and ESP32 integration | MCU developers |
| [testing-and-troubleshooting_en.md](testing-and-troubleshooting_en.md) | Tests and diagnostics | Test/field work |

## Current capability boundary

- Target: 128×64 SSD1306 or SSD1315 over I²C.
- Available BSP: Linux `/dev/i2c-*`.
- Graphics core: U8g2 C API with a full-screen 1-bit framebuffer.
- Drawing layer: pixels, lines, rectangles, circles, triangles, arcs, progress
  bars, XBM, 8-bit grayscale bitmaps, and UTF-8 text.
- Grayscale: 4×4 Bayer spatial dithering, not true multi-level OLED pixels.
- Refresh: `present()` sends the complete framebuffer; there is no dirty
  rectangle implementation.
- MCU: platform dependencies are isolated, but STM32 HAL and ESP-IDF BSPs are
  not yet included.

## Key source entry points

- `src/display/oled_display.hpp`: device lifecycle and U8g2 callbacks.
- `src/display/oled_canvas.hpp`: application drawing API.
- `src/bsp/i2c/bsp_i2c.hpp`: portable I²C contract.
- `src/app/oled_demo_app.cpp`: monitoring, graphics, and grayscale pages.
- `src/main.cpp`: Linux arguments, composition, and main loop.
- `src/tests/oled_core_tests.cpp`: fake-I²C and framebuffer tests.
