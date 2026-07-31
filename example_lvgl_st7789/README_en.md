# example_lvgl_st7789

[中文](README_cn.md) | **English**

Linux userspace LVGL 9.2.2 demo for a 1.54-inch, 240x240 ST7789 SPI LCD. The
project follows the cross-compilation setup used by `example_icm42688` and keeps
Linux device access separate from the LCD controller and LVGL adapter.

## Layers

```text
App/lvgl_demo_app
        |
Display/LvglSt7789Display       LVGL flush callback and RGB565 byte swap
        |
HardWare/St7789                controller initialization and address windows
        |
Peripheral/SpiBus + OutputPin  BSP interfaces
        |
Linux spidev + GPIO chardev    platform implementation
```

The ST7789 driver only depends on `bsp::SpiBus` and `bsp::OutputPin`. A new
platform can therefore replace the Linux implementations without changing the
controller or LVGL layers.

## What the LVGL example displays

After successful initialization, the 240x240 screen should show:

- a dark blue-black background;
- a green `LVGL + ST7789` title near the top;
- a light `Linux SPI BSP` status label in the center;
- a blue `Test` button near the bottom.

The UI is created by `app::createDemoUi()` in
`src/App/lvgl_demo_app.cpp`. The button already has an `LV_EVENT_CLICKED`
callback that changes the center label to `LVGL is running`. However, this
project does not register a touch, mouse, encoder, or key input device yet, so
the button cannot be operated on the board until an LVGL input BSP is added.

The program intentionally prints nothing after a successful startup. No error
and a process that keeps running means it has entered the LVGL event loop; it
does not prove that the physical SPI/GPIO wiring is correct.

## How LVGL is used

### Initialization and call flow

The simplified sequence in `src/main.cpp` is shown below; `panel_config`
represents the geometry and rotation values parsed from the command line:

```cpp
requireStatus(spi.init(), "SPI init");
requireStatus(dc.init(true), "D/C GPIO init");
requireStatus(reset->init(true), "reset GPIO init");
requireStatus(backlight->init(false), "backlight GPIO init");

hardware::St7789 panel(spi, dc, *reset, *backlight, panel_config);
requireStatus(panel.init(), "ST7789 init");

lv_init();
display::LvglSt7789Display display(panel, buffer_lines);
display.init();
app::createDemoUi();
```

Runtime data flow:

```text
LVGL widget changes / screen refresh
                |
                v
LvglSt7789Display::flushCallback(area, RGB565 pixels)
                |
                v
swap each RGB565 pixel to the ST7789 wire byte order
                |
                v
St7789::setAddressWindow() + writePixelBytes()
                |
                v
LinuxSpiBus -> /dev/spidevB.C -> LCD
```

`LvglSt7789Display::init()` performs the LVGL display-port setup:

1. Calls `lv_display_create(width, height)`.
2. Selects `LV_COLOR_FORMAT_RGB565`.
3. Registers `flushCallback()` with `lv_display_set_flush_cb()`.
4. Registers a partial rendering buffer with `lv_display_set_buffers()`.
5. The flush callback sends the updated rectangle to ST7789 and then calls
   `lv_display_flush_ready()` so LVGL can reuse the buffer.

The default partial buffer is 24 lines. At 240 pixels and RGB565, the LVGL draw
buffer is `240 * 24 * 2 = 11,520` bytes. The adapter also keeps a transmit buffer
of the same size for byte swapping, so the two buffers normally use about 23 KB.
Use `--buffer-lines` to trade memory for larger or smaller SPI flushes.

### LVGL tick and event loop

LVGL is configured with `LV_USE_OS LV_OS_NONE`, so the application supplies the
time base and runs LVGL from one thread:

```cpp
while (true) {
    lv_tick_inc(elapsed_ms);
    uint32_t wait_ms = lv_timer_handler();
    std::this_thread::sleep_for(...);
}
```

`lv_tick_inc()` advances LVGL time for animations and timers.
`lv_timer_handler()` updates widgets, renders invalidated areas, and eventually
invokes the display flush callback. Keep LVGL API calls on this thread unless a
locking strategy is added; LVGL objects should not be modified concurrently
from arbitrary worker threads.

### Adding your own interface

Edit `src/App/lvgl_demo_app.cpp`, preferably keeping device access out of this
file. Create widgets after `lv_init()` and display initialization. For example:

```cpp
void createDemoUi()
{
    lv_obj_t* screen = lv_screen_active();

    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_center(label);
}
```

For changing a widget later, retain its `lv_obj_t*`, pass it as callback user
data, or store it in an application-owned UI structure. Do not call SPI or
ST7789 functions directly from normal UI code; invalidating/changing LVGL
objects is enough, and the registered flush callback handles the LCD transfer.

LVGL compile-time options are in `lv_conf.h`. This project uses 16-bit color,
the built-in allocator, no LVGL OS layer, warning-level logging, and the
Montserrat 20 font used by the title.

## Wiring

### Before connecting

- Power the board off before wiring the LCD.
- This project assumes **3.3 V SPI/GPIO logic**. Do not connect a 5 V logic
  signal directly to the ST7789.
- A pin labelled `SCL` on this SPI module means SPI clock, and `SDA` means SPI
  MOSI. They are not I2C signals.
- The display is write-only in this project, so MISO is not used.

### Signal mapping

| LCD pin | Linux board signal | Notes |
| --- | --- | --- |
| VCC | 3.3 V | Use 3.3 V unless the exact module datasheet explicitly permits another supply |
| GND | GND | Common ground |
| SCL/SCK/CLK | SPI SCLK | Hardware SPI clock, mode 0 |
| SDA/MOSI/DIN | SPI MOSI | Hardware SPI transmit data |
| CS | SPI CS0 or CS1 | Must match the chip-select number in `/dev/spidevB.C` |
| DC/RS/A0 | Free GPIO output | Required; selects command or pixel data |
| RES/RST | Free GPIO output | Active low; recommended during bring-up |
| BL/LED | Backlight control | Active high in this program; see the warning below |

Connection template:

```text
ST7789 LCD                         Linux board
----------                         -----------
VCC       -----------------------> 3V3
GND       -----------------------> GND
SCL/SCK   -----------------------> SPIx_SCLK
SDA/MOSI  -----------------------> SPIx_MOSI
CS        -----------------------> SPIx_CS0       -> /dev/spidevB.0
DC        -----------------------> free GPIO      -> --dc <line-offset>
RST       -----------------------> free GPIO      -> --reset <line-offset>
BL        -----------------------> backlight ctrl -> --backlight <line-offset>
```

`B` is the Linux SPI bus number. The number after the dot is the chip select;
for example, `/dev/spidev0.0` is bus 0, CS0, while `/dev/spidev1.1` is bus 1,
CS1. Physical header pin names and numbers are board-specific, so verify them
against the board schematic and active device tree.

> **Backlight warning:** some LCD modules include a backlight resistor or
> transistor, while others expose the LED directly. Do not source an unknown
> backlight current directly from a SoC GPIO. If the module documentation is not
> clear, power BL through the recommended resistor/transistor circuit. If BL is
> safely tied to 3.3 V, omit `--backlight`.

### Finding the correct Linux devices

The SPI controller, pinmux, and `spidev` node must be enabled in the board's
device tree. On the target board, check:

```sh
ls -l /dev/spidev* /dev/gpiochip*
gpioinfo
```

For each control signal, select a free GPIO line and note both its gpiochip path
and line offset. All three control signals normally belong to the same gpiochip
because this application accepts one `--gpiochip` argument.

This project uses the Linux GPIO character-device ABI. Values passed to `--dc`,
`--reset`, and `--backlight` are **line offsets inside the selected gpiochip**,
not physical header-pin numbers and not legacy global sysfs GPIO numbers.

The numbers below are only an example; they are **not universal board pins**:

```text
/dev/gpiochip0 line 13 -> LCD DC
/dev/gpiochip0 line 12 -> LCD RST
/dev/gpiochip0 line 14 -> LCD BL
```

## Build on Windows

The default matches `example_icm42688`: `arm-none-linux-gnueabihf`, Release,
hard-float and static linking. The first configure downloads the pinned LVGL
v9.2.2 source into the build directory.

```powershell
cd example_lvgl_st7789
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build -j 8
```

If the compiler is not discoverable, provide its `bin` directory:

```powershell
cmake -G "MinGW Makefiles" -S . -B build `
  -DTOOLCHAIN_PATH=C:/path/to/arm-none-linux-gnueabihf/bin
cmake --build build -j 8
```

For offline builds, download or vendor LVGL separately and configure with:

```powershell
cmake -G "MinGW Makefiles" -S . -B build `
  -DLVGL_SOURCE_DIR=C:/path/to/lvgl
```

The executable is `build/src/example_lvgl_st7789`.

## Board setup

The kernel must expose both SPI and GPIO character devices. Recheck them before
running:

```sh
ls -l /dev/spidev* /dev/gpiochip*
gpioinfo
```

The application needs permission to open the selected devices. For initial
bring-up, run as root; for production, add an appropriate udev rule or device
group instead.

Example using `/dev/spidev0.0`, gpiochip0 line 13 for D/C, line 12 for reset,
and line 14 for backlight. Replace these illustrative line offsets with the
values found for the actual board:

```sh
chmod +x example_lvgl_st7789
./example_lvgl_st7789 \
  --spi /dev/spidev0.0 \
  --spi-hz 40000000 \
  --gpiochip /dev/gpiochip0 \
  --dc 13 --reset 12 --backlight 14
```

Only D/C is mandatory. If reset or backlight is hard-wired, omit the respective
option:

```sh
./example_lvgl_st7789 --spi /dev/spidev0.0 --gpiochip /dev/gpiochip0 --dc 13
```

All panel geometry can be adjusted without recompiling:

```sh
./example_lvgl_st7789 --dc 13 \
  --width 240 --height 240 \
  --rotation 90 --x-offset 0 --y-offset 0 \
  --buffer-lines 24
```

Use `--help` to list every option. If colors are correct but the image is
shifted, adjust the X/Y offsets. If the panel is unstable at 40 MHz, first try
`--spi-hz 20000000` or `--spi-hz 10000000`.

For the first power-on, starting at 10 MHz is recommended:

```sh
./example_lvgl_st7789 --spi /dev/spidev0.0 --spi-hz 10000000 \
  --gpiochip /dev/gpiochip0 --dc 13 --reset 12 --backlight 14
```

## Current scope

This first version provides display output only. It creates an LVGL screen and
a button widget, but no touch/input BSP is registered yet, so the button is a
visual test until an input device is added.
