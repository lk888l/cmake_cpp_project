# Testing and Troubleshooting

[中文](testing-and-troubleshooting_cn.md) | **English**

## 1. Test by layer

OLED failures usually originate in one of four layers. Validate them separately:

1. Pure framebuffer algorithms: drawing, clipping, fonts, and grayscale.
2. U8g2 translation: initialization commands, control bytes, and packetization.
3. BSP/operating system: device node, address, permissions, and I²C ioctl or HAL.
4. Physical hardware: wiring, pull-ups, supply, controller model, and panel offset.

When the panel stays dark, do not immediately change drawing algorithms. First determine whether initialization succeeded, the device ACKs, and the framebuffer contains pixels.

## 2. Host unit tests

The host configuration builds only `oled_core` and the fake I²C transport, not the Linux executable:

```powershell
cmake -S example_oled_portable -B example_oled_portable/build-local `
  -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=local `
  -DBUILD_TESTING=ON

cmake --build example_oled_portable/build-local -j 8
ctest --test-dir example_oled_portable/build-local --output-on-failure
```

`oled_core_tests` currently covers:

- SSD1306 initialization producing I²C transactions;
- every SSD13xx packet starting with control byte `0x00` or `0x40`;
- logical size 128×64 and framebuffer size 1024 bytes;
- BSP write failures becoming `transport_error`;
- failed initialization not setting `initialized_`;
- deterministic Bayer coverage at intensity 0, 128, and 255;
- out-of-range lines, rectangles, circles, and arcs not corrupting the framebuffer;
- UTF-8 width handling and grayscale text producing pixels;
- logical dimensions 64×128 after 90° rotation;
- SSD1315 setup producing valid initialization traffic through the fake BSP.

These tests cannot prove the physical panel model, wiring, or electrical timing.

## 3. Recommended additional tests

Before production release, add:

- framebuffer golden images or hashes for all three demo pages;
- monotonic coverage across all intensity values from 0 to 255;
- `drawGrayBitmap()` stride, clipping, and null-pointer cases;
- every `DisplayStatus` to `I2CStatus` error mapping;
- 64-byte aggregation-buffer boundaries and an intentional overflow;
- grayscale-text pattern consistency after rotation;
- memory and handle-leak checks over tens of thousands of `present()` calls;
- MCU hardware stress tests at both 100 and 400 kHz.

## 4. Diagnostic data

An initialization failure log should include:

```text
platform
bus identifier
7-bit address
controller profile
rotation
I2CStatus
DisplayStatus
```

Do not print the entire framebuffer in routine logs. For image diagnosis, save the 1024 bytes separately or convert them to a PBM file.

Useful MCU counters:

- initialization attempts;
- successfully presented frames;
- I²C timeouts;
- NACK/bus errors;
- bus recoveries;
- maximum `present()` duration.

## 5. Common failures

### `/dev/i2c-3` does not exist

Possible causes:

- the kernel lacks `i2c-dev`;
- the I²C controller is disabled in the device tree;
- the actual bus number is not 3;
- the pinmux is not configured.

Inspect:

```sh
ls /sys/class/i2c-dev
ls -l /dev/i2c-*
dmesg | grep -i i2c
```

Do not derive Linux `/dev/i2c-N` numbering only from the controller index in the SoC manual.

### `not_found`

The Linux BSP reports this when opening the device node fails or the lower layer returns `ENOENT`, `ENODEV`, or `ENXIO`. Check the path and kernel device first; this is not a drawing error.

### `transport_error`

This means an I²C write transaction failed. Typical causes:

- wrong 0x3C/0x3D address;
- swapped SDA/SCL or no common ground;
- incorrect supply voltage;
- missing or excessively strong pull-ups;
- another device holding the bus low;
- the panel lost power while the process continued refreshing.

Use an oscilloscope or logic analyzer to verify START, address ACK, control byte, and payload. Seeing an SCL waveform alone does not prove the slave ACKed.

### Initialization succeeds but the screen is black

Check in order:

1. Does `--controller ssd1306` or `ssd1315` match the hardware?
2. Does the program call power-save immediately after `present()`? `--once` exits and turns the panel off quickly.
3. Is contrast too low?
4. Did the page draw only intensity 0?
5. Does the module require an external RESET signal?
6. Is the controller actually an SH1106 or another model?

### Image shifted or edge clipped

This normally indicates a controller/panel geometry mismatch. An SH1106, for example, has 132 display-memory columns but often exposes 128 visible columns, and some modules require an offset. Do not add an application-wide X constant. Select or add the correct U8g2 display setup.

### Upside-down or mirrored image

First test `--rotation`. If coordinates become correct but text remains mirrored, the controller setup or module scan direction may differ. Fix this in the display profile.

### Garbled or missing text

- Source strings must be UTF-8.
- The selected font must contain the glyph.
- A numeric-only font cannot render ordinary letters.
- The Y coordinate passed to `drawText()` is the baseline; a small value can place the glyph above the screen.
- Chinese requires a Chinese font or a generated subset.

### Grayscale looks like a checkerboard

That texture is expected from spatial dithering. Improve appearance by using intermediate intensity only in larger fills, using 255 for critical text and thin lines, selecting a higher-resolution panel, or switching to a true grayscale controller such as SSD1327 when smooth grayscale is required.

### Grayscale pattern flickers

The Bayer pattern itself does not change by frame. Flicker indicates another issue:

- page logic alternates clearing and drawing at an irregular cadence;
- several tasks modify the framebuffer without synchronization;
- supply voltage drops or the connection is intermittent;
- I²C errors leave partial frames;
- the application changes intensity or pattern coordinates each frame.

### `buffer_overflow`

The U8g2 byte callback aggregates data into a 64-byte buffer. Current SSD1306/SSD1315 CAD chunks normally stay within it. If a new controller or changed packetization triggers overflow:

1. Record the accumulated length of each SEND sequence.
2. Confirm START and END messages are paired.
3. Increase `transfer_buffer_` only after checking MCU stack/RAM.
4. Never silently discard overflow data; that produces hard-to-diagnose corruption.

### MCU firmware size grows unexpectedly

Inspect the linker map:

- Was a large or complete Chinese font retained?
- Did `drawArc()` pull in double-precision `sin`/`cos`?
- Did `fillCircle()` pull in `sqrt`?
- Are `-ffunction-sections`, `-fdata-sections`, and `--gc-sections` missing?
- Are fonts placed in individually collectable sections?

Do not delete `u8g2_fonts.c` merely because the source file is large. Trim according to retained symbols and sections.

### MCU RAM is insufficient

The largest explicit buffers are the 1024-byte framebuffer and 1024-byte text scratch buffer. Optimize in this order:

1. Remove the scratch path when grayscale text is unnecessary.
2. Make scratch storage a caller-provided shared buffer.
3. Use U8g2 page-buffer mode, trading RAM for repeated drawing passes.
4. Reduce task stacks only after measuring their high-water marks.

Do not put the framebuffer in a function-local stack array.

## 6. Logic-analyzer criteria

A typical SSD13xx I²C write is:

```text
START
7-bit address + W
ACK
0x00 or 0x40
ACK
payload...
STOP
```

- `0x00` introduces commands and parameters.
- `0x40` introduces framebuffer data.

If the control byte and payload are split into separate transactions, the SSD1306 will not interpret the second transaction as intended. This is why one `I2CDevice::write()` call must transmit its entire input buffer continuously.

## 7. Release acceptance

- Debug and Release both build successfully.
- All host CTest cases pass.
- Cross-compiled ELF architecture, ABI, and static/dynamic policy match the root filesystem.
- SSD1306 and SSD1315 are each tested on real matching hardware.
- Rotation 0° and the actual product mounting orientation are tested.
- System startup, process restart, sleep/wake, and power-loss recovery are tested.
- A 24-hour refresh stress test passes.
- Shared-bus concurrency with sensors passes.
- Final flash, RAM, task stack, and per-frame duration are recorded.
