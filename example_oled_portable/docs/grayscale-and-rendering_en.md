# Grayscale, Framebuffer, and Rendering Details

[中文](grayscale-and-rendering_cn.md) | **English**

## 1. Why the SSD1306 has no true grayscale

The SSD1306/SSD1315 configuration used by this project exposes a 128×64 monochrome framebuffer. Each display-memory pixel has one bit: `0` is off and `1` is on. The controller's contrast command changes the drive strength of the whole panel; it cannot assign a different brightness to every pixel within one frame.

This project accepts an intensity from 0 to 255 to give applications a uniform visual-intensity API. Intermediate values are converted into black-and-white dot patterns with different spatial densities. This does not change the hardware pixel depth.

## 2. 4×4 Bayer spatial dithering

The current threshold matrix is:

```text
 0   8   2  10
12   4  14   6
 3  11   1   9
15   7  13   5
```

Intensity is first converted to a coverage level from 0 to 16:

```text
level = (intensity * 16 + 254) / 255
```

A pixel at `(x, y)` is lit when:

```text
matrix[y mod 4][x mod 4] < level
```

Consequently:

- intensity 0 lights 0/16 dots;
- intensity about 64 lights about 5/16 dots;
- intensity 128 lights 8/16 dots;
- intensity about 192 lights about 13/16 dots;
- intensity 255 lights 16/16 dots.

The matrix is anchored to absolute coordinates, so the same intensity produces the same pattern in every frame. It does not flicker like temporal PWM and requires no additional refresh bandwidth.

## 3. Visual characteristics

Advantages:

- Each frame is static and does not depend on a strict frame rate.
- Only one 1-bit framebuffer is transmitted.
- Linux, ESP32, and STM32 produce deterministic, matching pixel patterns.
- It works well for progress bars, shadows, background regions, and small grayscale icons.

Limitations:

- The repeating 4×4 texture can be visible at close range on a 0.96-inch panel.
- Intermediate shades may make thin lines and small text look fragmented.
- Panel color, viewing angle, and pixel pitch affect perceived brightness.
- Spatial dithering is not equivalent to a true 4-bit grayscale controller such as the SSD1327.

Use intensity 255 for critical text, borders, and warning icons. Intermediate values are better suited to large fills, secondary text, and decorative hierarchy.

## 4. Framebuffer layout

U8g2 full-buffer mode uses 1024 bytes. The SSD1306 organizes display RAM in pages, with each page covering eight rows and 128 columns:

```text
page 0: y = 0..7,   128 bytes
page 1: y = 8..15,  128 bytes
...
page 7: y = 56..63, 128 bytes
```

Application code should not depend on this layout or write the memory directly. Use `OledCanvas`, which applies rotation and clipping. Raw framebuffer access should be limited to tests, captures, or a measured and validated high-performance effect.

## 5. Drawing modes

### Replacing patterned pixels

When pixels, lines, or filled geometry call `drawPixel()`, the intensity determines whether every target pixel is set to `1` or cleared to `0`. An intermediate-intensity fill therefore replaces old content instead of merely OR-ing dots into the existing framebuffer.

This means a later grayscale fill can erase foreground content. Draw pages in a stable order such as background, large fills, icons and text, then emphasized borders.

### Transparent text

Text follows a different path. The renderer saves the old framebuffer, draws a full-brightness glyph with U8g2's transparent font mode, and applies the Bayer mask only to pixels introduced by that glyph. Background pixels not covered by the glyph are preserved.

For dynamic text, clear the screen or the text rectangle before drawing each frame. A shorter new string will not automatically erase the part of the previous string that extends beyond it.

## 6. Bitmap formats

### XBM 1-bit bitmaps

`drawBitmap()` accepts XBM, LSB-first data. A monochrome icon can be exported as XBM and compiled as a `const uint8_t[]`. Each row occupies `ceil(width / 8)` bytes.

### Gray8 bitmaps

Every input pixel passed to `drawGrayBitmap()` occupies one byte:

```cpp
constexpr uint8_t gradient[8] = {
    0, 36, 72, 108, 144, 180, 216, 255
};
canvas.drawGrayBitmap(0, 0, 8, 1, gradient);
```

The implementation bounds-checks and writes through U8g2 for every pixel, prioritizing portability and deterministic behavior. Converting a full-screen grayscale image touches 8192 pixels per frame. On an MCU, pre-dither static assets into XBM or add a measured batch framebuffer converter for animation-heavy workloads.

## 7. Text and font size

U8g2 decodes UTF-8, but the selected font determines the available glyphs. The four fonts currently included are primarily for ASCII and numbers. Chinese text can be added in three ways:

1. Use an existing U8g2 Chinese font set. This is quick to develop but may consume substantial flash.
2. Generate a subset containing only product-required characters with the U8g2 font tools. This is usually best for MCUs.
3. Convert a small number of fixed Chinese labels to XBM icons. This does not provide dynamic text.

Keep `-fdata-sections` and linker `--gc-sections` enabled, then inspect the linker map to see which fonts are retained. The size of the `u8g2_fonts.c` source file does not represent final firmware usage.

## 8. Refresh bandwidth

At 400 kHz, one I²C bit takes 2.5 µs in theory. A 1024-byte framebuffer is at least 8192 bits, or about 9216 bits including ACKs, which is roughly 23 ms before commands, control bytes, operating-system calls, and HAL overhead.

Practical guidance:

- 1–10 Hz is comfortable for dashboards, menus, and sensor values.
- 20–30 Hz can support simple animation, but bus utilization and task jitter must be measured.
- For higher frame rates, prefer SPI, partial updates, or a smaller update region.

Spatial grayscale does not multiply the number of refreshes. Temporal grayscale would require several subframes, which is why this project does not use it.
