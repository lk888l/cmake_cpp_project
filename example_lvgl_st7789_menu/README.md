# LVGL ST7789 Three-Key Menu

[中文说明](README_cn.md) | [Architecture and extension guide](doc/architecture.md)

`example_lvgl_st7789_menu` is a standalone C++23 example for a 240 x 240
ST7789 display on embedded Linux. It combines the display path from
`example_lvgl_st7789` with the Linux GPIO v2 button path from
`example_gpio_button`, then adds a three-key input bridge, a testable menu state
machine, Chinese UI assets, and deliberately short LVGL animations.

The application is designed for a write-only SPI display with no touch panel.
All LVGL calls stay on the main thread. A GPIO worker collects the three keys
and only pushes events into a protected queue; the main loop drains that queue
before updating the menu.

## What appears on the display

The home screen is a looping card carousel. The selected card remains in the
center while part of the previous and next cards remains visible. It opens four
Chinese pages:

| Home card | Interaction |
| --- | --- |
| `圆环调节` | Edit an arc value from 0 to 100 in steps of 5, with animated value feedback |
| `开关互动` | Toggle breathing motion and focused-card highlighting |
| `动画实验` | Select slow/medium/fast motion and play or pause the small breathing/orbit animation |
| `按键状态` | Inspect pressed state, press count, most recent event, and held duration for all three keys |

The settings are demonstration state held in RAM. They are not persisted across
application restarts.

## Key map

| Physical key | Short action | Hold behavior |
| --- | --- | --- |
| Up | Move to the previous card/control or decrease an edited value | Repeats after 400 ms, every 100 ms by default |
| Down | Move to the next card/control or increase an edited value | Repeats after 400 ms, every 100 ms by default |
| Confirm | Enter a page, toggle a control, or enter/finish editing | Long press goes back; while editing it first leaves edit mode and keeps the current value |

Up and Down both move once as soon as they are pressed. Holding both at the same
time pauses directional repeat. Releasing either one resumes the remaining key
after one repeat period. Confirm press/release drives the tactile press
animation; a click activates and a long press goes back. The supplemental
`DoubleClick` event has no separate menu command.

On the home page, long-pressing Confirm plays boundary feedback instead of
leaving the application.

## Source layout

```text
example_lvgl_st7789_menu/
|-- CMakeLists.txt
|-- cmake/                       ARM Linux toolchain
|-- doc/architecture.md          ownership, data flow, and extension guide
|-- lv_conf.h.in                 Generated LVGL 9 configuration template
`-- src/
    |-- bsp/                     Linux spidev and output GPIO adapters
    |-- hardware/                ST7789 controller driver
    |-- display/                 LVGL partial-buffer display bridge
    |-- input/                   GPIO v2 manager, gestures, and queued key router
    |-- assets/fonts/            generated 16 px and 20 px Chinese font subsets
    |-- app/                     pure menu model and LVGL view/application
    |-- tests/                   host-side state-machine and routing tests
    `-- main.cpp                 CLI, resource wiring, and main loop
```

The project copies and adapts the reference implementations; it does not link
source files from either reference example.

## Hardware and wiring

### Requirements

- Embedded Linux with `/dev/spidevB.C`; the three key inputs require the GPIO
  character-device ABI v2.
- An ST7789 panel using 3.3 V SPI/GPIO logic, normally 240 x 240 pixels.
- Three momentary keys on GPIO inputs. The default wiring is active-low and
  requires pull-ups supplied by the board, device tree, or external resistors.
- Permission to open the selected SPI and GPIO devices.

The program does not configure pinmux or input bias. Do that in the device tree
or board configuration before launching the application.

The LCD output GPIO adapter intentionally remains the implementation inherited
from the display reference; only the button-input backend uses GPIO v2 events.

### Connection template

| Peripheral pin | Linux board signal | CLI option / note |
| --- | --- | --- |
| LCD VCC | 3.3 V | Check the exact module datasheet |
| LCD GND | GND | Common ground |
| LCD SCL/SCK | SPI SCLK | Hardware SPI mode 0 |
| LCD SDA/MOSI | SPI MOSI | MISO is unused |
| LCD CS | SPI CS | Must match `/dev/spidevB.C` |
| LCD DC/RS | Free GPIO output | `--gpiochip`, `--dc` (required) |
| LCD RST | Free GPIO output | `--reset` (optional, active-low) |
| LCD BL | Safe backlight control | `--backlight` (optional, active-high) |
| Up key | GPIO input to GND | `--key-gpiochip`, `--key-up` |
| Down key | GPIO input to GND | `--key-down` |
| Confirm key | GPIO input to GND | `--key-ok` |

For active-high keys, connect each key according to the board's safe 3.3 V
input circuit, provide a pull-down, and pass `--keys-active-high`.

> Some LCD modules expose the backlight LED directly. Do not source an unknown
> LED current from a SoC GPIO. Use the module's recommended resistor/transistor
> circuit, or omit `--backlight` when BL is safely hard-wired.

### GPIO line offsets

Every number passed to `--dc`, `--reset`, `--backlight`, `--key-up`,
`--key-down`, or `--key-ok` is a **line offset inside its selected gpiochip**.
It is not a physical header-pin number and not a legacy sysfs global GPIO
number. Find the actual mapping on the target board:

```sh
ls -l /dev/spidev* /dev/gpiochip*
gpioinfo
```

The display outputs use `--gpiochip`; the three input keys use
`--key-gpiochip`. These may name the same or different chips. Every selected
physical line must be unique and must not already be claimed by `gpio-keys`, an
LED driver, another process, or another kernel driver.

The following is illustrative only:

```text
/dev/gpiochip0 line 13 -> LCD DC
/dev/gpiochip0 line 12 -> LCD RST
/dev/gpiochip0 line 14 -> LCD BL
/dev/gpiochip1 line 25 -> Up key
/dev/gpiochip1 line 26 -> Down key
/dev/gpiochip1 line 27 -> Confirm key
```

## Build

### ARM Linux application on Windows

The default target is `arm-none-linux-gnueabihf`, Release, hard-float, and
static linking. `MinGW Makefiles` is the recommended generator for the GNU
cross-toolchain used by the sibling examples.

```powershell
cd example_lvgl_st7789_menu
cmake --preset arm-release
cmake --build --preset arm-release
```

The executable is `build/arm-release/src/example_lvgl_st7789_menu`. Always use
a fresh Release directory for the board: an old Debug LVGL build can saturate a
small SoC even when the source is otherwise correct.

The first online configure fetches the pinned LVGL v9.2.2 source. To avoid a
download, point to an existing matching LVGL source tree:

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DLVGL_SOURCE_DIR=C:/path/to/lvgl `
  -DLVGL_MENU_BUILD_TESTS=OFF
```

If the compiler is not on `PATH`, add
`-DTOOLCHAIN_PATH=C:/path/to/arm-none-linux-gnueabihf/bin`. A target sysroot can
be selected with `-DSYSROOT_PATH=C:/path/to/sysroot`.

Useful CMake options:

| Option | Default | Purpose |
| --- | --- | --- |
| `TARGET_PLATFORM` | `arm-none-linux-gnueabihf` | Select ARM Linux or `local` |
| `LVGL_MENU_BUILD_APP` | `ON` | Build the Linux display application and LVGL |
| `LVGL_MENU_BUILD_TESTS` | `ON` | Build testable core targets and tests |
| `LVGL_MENU_BUILD_HEADLESS_TESTS` | `OFF` | Build the optional LVGL first-frame test |
| `LVGL_MENU_BASE_HEAP_KIB` | `64` | Generated LVGL TLSF base-pool size |
| `LVGL_SOURCE_DIR` | empty | Use an existing LVGL tree instead of fetching v9.2.2 |
| `USE_STATIC_LINKING` | `ON` for ARM | Add static executable link options |
| `TOOLCHAIN_PATH` | empty | Cross-toolchain `bin` directory |
| `SYSROOT_PATH` | empty | Optional target sysroot |

### Host tests on Windows

Disabling the application avoids both the Linux-only sources and LVGL download.
The input and menu state machines remain ordinary host-testable C++.

```powershell
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Set `LVGL_MENU_BUILD_HEADLESS_TESTS=ON` in a separate local build to compile
LVGL and exercise a 240 x 240 Low-profile first frame with an 8-line buffer.

After an ARM Release build, these checks should report an ELF32 ARM hard-float
binary, static linkage, and no `INTERP` program header:

```sh
file build/arm-release/src/example_lvgl_st7789_menu
arm-none-linux-gnueabihf-readelf -h build/arm-release/src/example_lvgl_st7789_menu
arm-none-linux-gnueabihf-readelf -l build/arm-release/src/example_lvgl_st7789_menu
```

## Run on the board

All three key lines and the LCD D/C line are required. Reset and backlight are
optional when safely hard-wired.

```sh
chmod +x example_lvgl_st7789_menu
./example_lvgl_st7789_menu \
  --spi /dev/spidev0.0 --spi-hz 40000000 \
  --gpiochip /dev/gpiochip0 --dc 13 --reset 12 --backlight 14 \
  --key-gpiochip /dev/gpiochip1 --key-up 25 --key-down 26 --key-ok 27
```

For the first display bring-up, reduce the SPI clock to 10 MHz. Increase it
after color, rotation, offsets, and wiring are known to be correct:

```sh
./example_lvgl_st7789_menu \
  --spi /dev/spidev0.0 --spi-hz 10000000 \
  --gpiochip /dev/gpiochip0 --dc 13 \
  --key-gpiochip /dev/gpiochip1 --key-up 25 --key-down 26 --key-ok 27
```

For RV1103-class or similarly constrained boards, `auto` normally resolves to
the Low profile. This explicit command is useful while validating resources:

```sh
./example_lvgl_st7789_menu \
  --render-profile low --buffer-lines 8 --stats-interval-ms 2000 \
  --spi /dev/spidev0.0 --spi-hz 40000000 \
  --gpiochip /dev/gpiochip0 --dc 13 \
  --key-gpiochip /dev/gpiochip1 --key-up 25 --key-down 26 --key-ok 27
```

### Command-line reference

| Option | Default | Meaning |
| --- | --- | --- |
| `--spi <path>` | `/dev/spidev0.0` | SPI device |
| `--spi-hz <hz>` | `40000000` | SPI clock |
| `--gpiochip <path>` | `/dev/gpiochip0` | LCD control GPIO chip |
| `--dc <line>` | required | LCD D/C line offset |
| `--reset <line>` | omitted | LCD reset line offset |
| `--backlight <line>` | omitted | LCD backlight line offset |
| `--width <px>` / `--height <px>` | `240` / `240` | Panel geometry |
| `--x-offset <px>` / `--y-offset <px>` | `0` / `0` | Controller RAM offsets |
| `--rotation <deg>` | `0` | `0`, `90`, `180`, or `270` |
| `--render-profile <name>` | `auto` | Select `auto`, `low`, `balanced`, or `quality` |
| `--buffer-lines <n>` | profile | Override LVGL partial render-buffer height |
| `--target-fps <n>` | profile | Override refresh target from 1 to 60 FPS |
| `--lvgl-extra-heap-kib <n>` | profile | Override additional LVGL TLSF pool; `0` disables it |
| `--stats-interval-ms <ms>` | `0` | Periodic flush/timer/memory diagnostics; `0` disables |
| `--shutdown-timeout-s <s>` | `3` | Forced-exit deadline after the first termination signal |
| `--key-gpiochip <path>` | `/dev/gpiochip0` | Key input GPIO chip |
| `--key-up <line>` | required | Up key line offset |
| `--key-down <line>` | required | Down key line offset |
| `--key-ok <line>` | required | Confirm key line offset |
| `--keys-active-high` | off | Use active-high instead of active-low inputs |
| `--debounce-ms <ms>` | `25` | Edge debounce interval |
| `--long-press-ms <ms>` | `600` | Confirm long-press threshold |
| `--double-click-ms <ms>` | `250` | Gesture state-machine double-click window |
| `--repeat-delay-ms <ms>` | `400` | Up/Down hold delay before repeat |
| `--repeat-period-ms <ms>` | `100` | Up/Down repeat period |
| `--help`, `-h` | - | Print the current program usage |

Invalid numbers, missing required lines, unsupported rotation, duplicate
physical lines, and conflicting device assignments are rejected at startup.

## Motion behavior

Animation durations are centralized in `app::MotionTiming`:

- focus movement: 160 ms, ease-out;
- Confirm press: 2 px/card-border pulse in Low/Balanced, or 96% scale in Quality;
- Confirm release: 130 ms with overshoot;
- forward/back page transition: 220 ms directional slide;
- arc/value retargeting: 140 ms;
- breathing cycle: 900 ms.

Before starting a replacement animation, the application removes the older
animation on the same target. `auto` reads `/proc/meminfo` and online CPU count;
missing resource data safely selects Low. Low/Balanced never apply whole-card
scale or layered opacity. Quality enables them only when LVGL reports a large
enough contiguous block. Defaults are Low 8 lines/20 FPS/+32 KiB, Balanced
12/25/+64 KiB, and Quality 24/30/+128 KiB.

## Tests and board acceptance

Host tests cover render-profile thresholds/overrides/layer budgets, debounce and bounce recovery, long-press boundaries,
double-click event ordering, polarity, producer/consumer queue ordering,
immediate movement, repeat timing, delayed-frame no-burst behavior, opposite
direction handling, Confirm activation/back exclusivity, menu wrapping, value
boundaries, switch behavior, edit mode, and page return behavior.

On the board, also check behavior that a host test cannot prove:

- rapid alternating Up/Down presses and held-key repeat;
- holding Up and Down together, then releasing only one;
- short and long Confirm gestures on every page;
- repeated page entry/return without stuck focus or animation buildup;
- button-state telemetry while a key remains held;
- stable rendering at the chosen SPI clock without flashes or dropped releases;
- non-zero process exit when the GPIO worker fails or an SPI flush fails.

## Troubleshooting

| Symptom | Checks |
| --- | --- |
| `/dev/spidev*` or `/dev/gpiochip*` is missing | Enable the controller, pinmux, and character-device nodes in the device tree/kernel |
| `Permission denied` | Test as root, then configure a narrowly scoped udev rule or device group |
| `Device or resource busy` | Release the line from `gpio-keys`, LED drivers, kernel consumers, or another process |
| GPIO v2 ioctl reports `Invalid argument` | Confirm kernel GPIO uAPI v2 support and that the line offset is within the selected chip |
| Keys never react | Verify line offsets, input pinmux, pull-up/pull-down, common ground, and polarity flag |
| One press produces several actions | Inspect wiring/noise and increase `--debounce-ms`; ordinary Release + Click is an expected gesture sequence |
| Image is shifted or clipped | Correct `--width`, `--height`, `--rotation`, `--x-offset`, and `--y-offset` |
| Panel is unstable at 40 MHz | Try `--spi-hz 20000000` or `10000000`, then inspect wiring and signal integrity |
| Only a top strip renders and CPU reaches 100% | Rebuild with `arm-release`, use `--render-profile low --buffer-lines 8`, and inspect the startup/first-frame LVGL memory report |
| Ctrl+C does not return promptly | The first signal starts graceful shutdown and a deadline; press it again for immediate exit, or lower `--shutdown-timeout-s` |
| Chinese text is missing | Confirm both generated font C sources are linked through the font target and that the text is included in the subset charset |

## Chinese font assets and license

The checked-in 16 px body and 20 px title resources are subsets generated from
the locally installed `NotoSansSC-VF.ttf`. Noto Sans SC is licensed under the
SIL Open Font License 1.1. Keep that license and attribution when redistributing
or regenerating the font data. Normal application builds use the generated C
files and require neither the source TTF nor a font conversion tool.

The in-tree font notice is `src/assets/fonts/FONT_LICENSE.md`.

The exact regeneration workflow and subset maintenance rules are recorded in
[doc/architecture.md](doc/architecture.md#font-assets).
