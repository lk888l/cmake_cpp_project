# Linux and Buildroot Build and Runtime Guide

[中文](linux-buildroot_cn.md) | **English**

## 1. Target environment

The default target is 32-bit ARM Linux with the hard-float ABI and this toolchain prefix:

```text
arm-none-linux-gnueabihf-
```

Default runtime settings:

| Setting | Default |
| --- | --- |
| I²C device | `/dev/i2c-3` |
| 7-bit address | `0x3C` |
| Requested I²C clock | 400000 Hz |
| Controller | SSD1306 |
| Resolution | 128×64 |
| Rotation | 0° |
| Refresh period | 1000 ms |

On Linux, the actual `i2c-dev` bus frequency is normally set by the device tree and controller driver. `clock_hz` is passed to the BSP factory, but the current Linux implementation does not change the controller clock from user space. Confirm the real frequency in the board configuration.

## 2. Hardware connection

Typical four-pin I²C OLED wiring:

| OLED | Linux board | Notes |
| --- | --- | --- |
| GND | GND | A common ground is mandatory. |
| VCC | 3.3 V when allowed by the module specification | Do not infer 5 V tolerance from the pin label alone. |
| SCL | I²C SCL | Pull-up required. |
| SDA | I²C SDA | Pull-up required. |

Check whether the module already includes pull-ups. With several modules in parallel, ensure the combined pull-up resistance is not too low. The SoC pins must be configured for I²C, not ordinary GPIO.

The program does not drive a separate RESET pin. Four-pin modules usually rely on power-on reset. If a production module exposes RESET, implement the required sequence in platform startup or extend the display configuration.

## 3. Cross-compilation

Run from the repository root:

```powershell
cmake -S example_oled_portable -B example_oled_portable/build-arm `
  -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=OFF

cmake --build example_oled_portable/build-arm -j 8
```

CMake searches `PATH` and `C:/kk_software/toolchain/*arm-none-linux-gnueabihf*/bin`. To specify the location explicitly:

```powershell
-DTOOLCHAIN_PATH=C:/path/to/toolchain/bin
```

When a sysroot matching the target root filesystem is available:

```powershell
-DSYSROOT_PATH=C:/path/to/sysroot
```

`USE_STATIC_LINKING=ON` is the default. A Release build is stripped with `--strip-unneeded`. The resulting program is:

```text
example_oled_portable/build-arm/src/example_oled_portable
```

## 4. Verify the artifact

```powershell
arm-none-linux-gnueabihf-readelf -h `
  example_oled_portable/build-arm/src/example_oled_portable

arm-none-linux-gnueabihf-readelf -l `
  example_oled_portable/build-arm/src/example_oled_portable

arm-none-linux-gnueabihf-size `
  example_oled_portable/build-arm/src/example_oled_portable
```

Expected properties:

- `Class: ELF32`
- `Machine: ARM`
- hard-float EABI flags
- no `INTERP` program header for a static build

If a file exists on the board but executing it reports `not found`, inspect the ELF architecture and dynamic interpreter rather than only the filename. A static build commonly avoids a target root filesystem missing `/lib/ld-linux-armhf.so.3`.

## 5. Buildroot integration

### Direct-copy validation

```sh
scp example_oled_portable root@BOARD:/usr/bin/
ssh root@BOARD chmod +x /usr/bin/example_oled_portable
```

This is the fastest method for initial hardware validation.

### Buildroot package

For production images, create a custom package:

1. Put the source in an external tree or a versioned source archive.
2. Configure with the Buildroot-provided cross toolchain.
3. Install the executable into `/usr/bin`.
4. Enable kernel I²C support and `CONFIG_I2C_CHARDEV`.
5. Enable the target I²C controller and its pinmux in the device tree.

U8g2 is vendored, so the Buildroot build does not require network access.

## 6. Board preparation

List device nodes:

```sh
ls -l /dev/i2c-*
```

Scan bus 3:

```sh
i2cdetect -y 3
```

The module normally responds at `3c`, or occasionally `3d`. Scanning itself accesses the bus; do not use it casually when another device on that bus cannot tolerate SMBus-style probing.

Root access is acceptable for initial diagnosis. A production image should use udev/mdev permissions or a dedicated device-access group instead of broadly granting hardware access.

## 7. Command line

```text
--bus PATH
--addr ADDRESS
--period-ms 1..60000
--rotation 0|90|180|270
--controller ssd1306|ssd1315
--demo dashboard|graphics|grayscale
--once
-h, --help
```

Examples:

```sh
# Default system dashboard
./example_oled_portable

# SSD1315 at 0x3D, rotated 180 degrees
./example_oled_portable \
  --controller ssd1315 --addr 0x3d --rotation 180

# Render the grayscale test once, then exit
./example_oled_portable --demo grayscale --once
```

`--once` still initializes the panel, sends one frame, and enters power-save mode during shutdown. It is useful for checking the transaction and page generation, but the panel turns off quickly. Omit `--once` when visually inspecting a static page.

## 8. Demo pages

### `dashboard`

Linux data sources:

- `localtime_r()` for the time;
- `getifaddrs()` for the first non-loopback IPv4 address;
- `/proc/stat` for differential CPU utilization;
- `/proc/meminfo` for memory utilization.

The first CPU sample has no history, so an initial 0% reading is expected.

### `graphics`

Validates clipping, geometry, fonts, and intermediate-intensity patterns. If geometry is correct but text is not, inspect font linkage and the framebuffer before suspecting display initialization.

### `grayscale`

Displays multiple Bayer coverage levels from 0 to 255. The pattern must remain static. Whole-screen flicker usually indicates power, wiring, repeated clearing, task synchronization, or I²C errors—not intentional temporal grayscale.

## 9. Shutdown behavior

SIGINT and SIGTERM stop the main loop and then call:

```cpp
oled.setPowerSave(true);
```

The panel therefore turns off after a normal exit. Power loss, `SIGKILL`, or a process crash cannot execute this cleanup path.
