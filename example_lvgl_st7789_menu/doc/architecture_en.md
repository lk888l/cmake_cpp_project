# Architecture and Extension Guide

[中文](architecture_cn.md) | **English**

This document records the runtime ownership boundaries and the safe way to
extend `example_lvgl_st7789_menu`. The source is authoritative when a future
change intentionally alters an interface.

## Design goals

- Run a 240 x 240 ST7789 and three GPIO keys from Linux userspace.
- Keep the GPIO worker independent from LVGL and keep all LVGL calls on one
  owner thread.
- Keep button timing, input routing, and menu navigation testable without Linux
  devices or LVGL.
- Give short, replaceable animations a responsive feel on a partial-buffer SPI
  display.
- Preserve clear BSP, hardware, display, input, and application boundaries.

The current version has no touch input, runtime pin reconfiguration,
persistence, PWM brightness, or dynamic page registry.

## Targets and ownership

```text
example_lvgl_st7789_menu (main/CLI/lifecycle)
  |-- menu_app ---------> menu_ui + menu_ui_fonts + LVGL + menu_input_core
  |-- menu_ui ----------> replace-on-retarget LVGL motion primitives
  |-- menu_runtime_core -> resource detection + pure RenderPolicy
  |-- menu_lvgl_runtime -> additional TLSF-pool storage
  |-- menu_input_linux -> menu_input_core + Threads
  |-- menu_display -----> menu_st7789 + LVGL
  |-- menu_st7789 ------> menu_bsp
  `-- LVGL

host tests
  |-- pure tests -------> menu_input_core + menu_runtime_core + MenuModel
  `-- optional headless -> menu_app + LVGL, no Linux display devices
```

| Layer | Main responsibility | Must not own |
| --- | --- | --- |
| `bsp` | Linux spidev writes, GPIO output line lifecycle, status conversion | LVGL widgets or menu policy |
| `hardware` | ST7789 init, orientation, address window, pixel transfer, backlight command | Linux file descriptors beyond BSP interfaces |
| `display` | LVGL display creation, RGB565 partial buffers, flush callback, flush failure state | Menu navigation or GPIO input |
| `input` core | Debounce/gestures, thread-safe raw-event queue, repeat policy, telemetry snapshots | LVGL calls |
| `input` Linux | GPIO v2 line requests, `epoll`, `timerfd`, `eventfd`, worker lifecycle | Menu/page decisions |
| `app` model | Pure page, focus, edit, value, and switch state transitions | Threads, Linux devices, LVGL |
| `ui` | Reusable LVGL animation lifecycle and timing contract | Menu state or hardware access |
| `app` presentation | LVGL object tree, styles, focus/press/page/value rendering | GPIO reads or callback-thread work |
| `runtime` | Resource detection, render-policy resolution, additional LVGL pool lifetime | Chip-model checks or widget decisions |
| `main` | Parse/validate CLI, construct resources, bridge callbacks, run and stop the loop | Widget-specific business rules |

`LVGL_MENU_BUILD_APP=OFF` normally skips `bsp`, `hardware`, `display`, fonts,
app view, Linux input backend, and LVGL itself. It still permits the
host-testable core to build. `LVGL_MENU_BUILD_HEADLESS_TESTS=ON` deliberately
adds only LVGL/fonts/app view for the optional first-frame render test.

## Runtime data flow

```text
GPIO electrical edge
  -> Linux GPIO v2 event fd (kernel monotonic timestamp)
  -> ButtonManager worker: epoll + ButtonStateMachine
  -> callback: ThreeKeyLvglInput::push()
  -> mutex + deque
  -> main loop: ThreeKeyLvglInput::pump()
  -> InputAction sink on the LVGL owner thread
  -> MenuApplication::handleAction()
  -> pure MenuModel transition + direct presentation update
  -> LVGL invalidated areas
  -> LvglSt7789Display flush callback
  -> St7789 address window + LinuxSpiBus write
```

The callback passed to `ButtonManager` is intentionally small. It maps the
configured string ID to `PhysicalButton` and calls `push`; it never invokes
`lv_*`, waits for animation, formats a page, or performs SPI I/O.

Before constructing LVGL, `main` resolves a resource-independent render policy
from online CPU count and `/proc/meminfo`, then applies explicit CLI overrides.
The main loop owns these recurring operations:

1. drain queued input with `ThreeKeyLvglInput::pump()`;
2. give current key snapshots to the button-status page;
3. advance application-local time through `MenuApplication::tick()`;
4. call the LVGL timer handler and wait only for its bounded next interval;
5. inspect GPIO worker and display-flush error state.

If the GPIO worker reports a fatal error or the display bridge records an SPI
flush failure, the loop stops, releases resources in reverse ownership order,
and returns a non-zero process status.

The first SIGINT/SIGTERM requests normal shutdown and arms a SIGALRM deadline.
Handlers are installed without `SA_RESTART`, so an interrupted SPI ioctl can
return control to the loop. A second termination signal or expired deadline
uses `_exit()`; this is intentionally the last-resort path when LVGL or a driver
never returns.

## Linux GPIO event backend

`gpio_button::ButtonManager` retains the public semantics of the standalone
button example:

```cpp
struct ButtonConfig {
    std::string id;
    std::string chip_path{"/dev/gpiochip0"};
    unsigned int line_offset{};
    bool active_low{true};
    MonotonicDuration debounce{25ms};
    MonotonicDuration long_press{600ms};
    MonotonicDuration double_click_window{250ms};
};
```

Each configured key is requested as a Linux GPIO v2 input with rising- and
falling-edge events. Active-low inputs request the internal pull-up; the
active-high override requests the internal pull-down. Luckfox's Linux 5.10
Rockchip GPIO driver accepts those bias flags without forwarding them to
pinctrl. If the device-tree `compatible` identifies RV1103 or RV1106, the
backend additionally writes the matching two-bit IOC pull field through
`/dev/mem` and verifies the register value. The compatible check, GPIO-chip
bank name, line range, register table, and read-back all have to agree before
this fallback can succeed. Other platforms never enter the direct-register
path. The backend then reads the initial level and registers all line fds plus
one timerfd and one stop eventfd in a single epoll instance.
Kernel event timestamps remain the source of gesture duration; `CLOCK_MONOTONIC`
absolute timerfd deadlines complete pending debounce and long-press decisions
even if no new edge arrives.

This v2 event path applies to the three input keys. The LCD D/C, reset, and
backlight outputs intentionally retain the character-device output adapter from
the ST7789 reference instead of broadening this example into a display-BSP
migration.

The state machine emits `Press`, `Release`, `Click`, `DoubleClick`, and
`LongPress`. A release at the exact long-press boundary is classified as a long
press and does not also activate the control. A key already down when the
application starts establishes the backend's initial stable level but does not
emit a synthetic press, click, or long press; its first release has an unknown
held duration of zero.

The Linux backend validates unique IDs, unique `(chip_path, line_offset)` pairs,
line ranges, and lifecycle state. Startup errors throw synchronously. A later
worker failure is exposed through `lastError()` so the main loop can exit.

## Main-thread input bridge

`input::ThreeKeyLvglInput` defines the physical and semantic boundaries:

```cpp
enum class PhysicalButton { Up, Down, Confirm };

enum class InputAction {
    Previous,
    Next,
    ConfirmPressed,
    ConfirmReleased,
    Activate,
    Back,
};
```

`push()` is the producer-side API and only appends a raw event under a mutex.
`pump()` swaps the shared deque into a main-thread-local deque, preserves FIFO
order, updates `ButtonSnapshot`, and invokes action/telemetry sinks on the main
thread. Therefore even the button-status page is updated without crossing the
LVGL thread boundary.

Navigation press is immediate. Repeat begins at `repeat_delay` and runs at
`repeat_period`; a delayed main-loop frame emits at most one repeat instead of
catching up with a burst. When Up and Down are both down, repeat is paused. When
one is released, the remaining direction resumes after one period.

Confirm maps as follows:

| Raw event | Semantic action |
| --- | --- |
| `Press` | `ConfirmPressed` for immediate visual compression |
| `Release` | `ConfirmReleased` for release/overshoot animation |
| `Click` | `Activate`, unless a long press was already seen |
| `LongPress` | `Back`, once per hold |
| `DoubleClick` | ignored as a separate command |

## Menu model

`app::MenuModel` is a pure state machine. `MenuSnapshot` is the complete
observable state used by tests and the LVGL view:

- current `MenuPage`: Home, Arc, Switches, Animation, or Buttons;
- `MenuMode`: Browse or Edit;
- home selection and detail focus;
- arc value, two switch values, animation speed, and play/pause state;
- monotonic revision and home-boundary feedback counter.

### Home

Previous/Next wraps across four items. Activate enters the selected detail page
with focus zero and Browse mode. Back increments boundary feedback so the view
can acknowledge the gesture without leaving the program.

### Arc

Activate toggles Browse/Edit. In Edit, Previous/Next subtracts/adds five and
clamps at 0/100. Back first leaves Edit while keeping the current value; a later
Back returns Home.

### Switches

Previous/Next wraps between the two switches. Activate toggles the focused
switch. The first switch enables the animation-page breathing pulse; the
second enables accent highlighting on focused rows and home cards. Back
returns Home.

### Animation

Previous/Next wraps between the speed selector and play/pause control. Activate
on speed enters or leaves Edit; while editing, Previous/Next cycles slow,
medium, and fast. Activate on the second control toggles play/pause. Back first
leaves Edit, then returns Home.

### Buttons

The page is read-only and receives the three `ButtonSnapshot` values. Back
returns Home.

## LVGL view and motion

`app::MenuApplication::create(lv_obj_t*, const RenderPolicy&)` builds the view once on the LVGL owner
thread. `handleAction(InputAction)` drives the menu model and immediate press
feedback. `tick(elapsedMs)` advances the small application animation and
refreshes time-dependent status without introducing another UI thread.

`ui::MotionTiming` is the single timing contract:

| Motion | Duration | Behavior |
| --- | --- | --- |
| Home focus frame | 160 ms | ease-out to the newest static row |
| Confirm down | 70 ms | 2 px/border pulse in Low/Balanced; 96% scale in Quality |
| Confirm up | 130 ms | overshoot back to full size |
| page forward/back | 220 ms | directional slide |
| arc/value change | 140 ms | retargeted interpolation |
| breathing animation | 900 ms | bounded repeating cycle |

`ui::retargetAnimation` deletes the old animation for the same object/property
before starting the replacement. Never queue one animation per repeat event.
The pure `MenuModel` is the only navigation state source; the presentation does
not mirror it into an LVGL encoder/group. Home rows remain static and only the
independent focus frame moves, bounding per-transition damage. Large shadows,
full-screen translucent layers, and permanent full-screen invalidation are
intentionally avoided for SPI bandwidth.

The display bridge uses RGB565 and profile-controlled partial-buffer and refresh
periods. It always completes the LVGL flush handshake, but keeps
the first SPI error sticky so a later successful area flush cannot hide it.
The main loop observes that error and exits cleanly with a non-zero status.

## CLI validation and resource lifetime

Parsing happens before Linux resources are opened. Normal operation requires
LCD D/C and all three key lines. `--benchmark-home` deliberately requires only
the display path, cycles Home focus deterministically, reports SPI metrics, and
exits. Numeric durations and sizes must be positive where the underlying
interface requires it; rotation is limited to 0/90/180/270. A physical GPIO
line cannot serve two roles when both roles use the same selected gpiochip
path.

Construction order is SPI and display GPIO outputs, ST7789/display bridge,
LVGL application, input router, then `ButtonManager`. Teardown stops and joins
the GPIO worker before destroying the queue sink or any LVGL objects.

The default display values are 240 x 240, offset 0/0, profile-selected buffer
and FPS, and 40 MHz SPI. The default input values are active-low with internal
pull-up, 25 ms debounce, 600 ms long press, 250 ms double-click window, 400 ms
repeat delay, and 100 ms repeat period. See the README CLI table for every
option.

## Adaptive render policy and memory

`RenderProfile` is `Auto`, `Low`, `Balanced`, or `Quality`; it never names a
chip. Auto safely falls back to Low when any required resource field is absent.
Otherwise, single-core/128 MiB total/64 MiB available boundaries select Low;
two-core/256 MiB total/128 MiB available boundaries select Balanced; larger
systems select Quality. Any matching lower-resource condition wins.

| Profile | Buffer | Target | Added TLSF pool | Layer behavior |
| --- | ---: | ---: | ---: | --- |
| Low | 40 lines | 30 FPS | 32 KiB | whole-control layers disabled; dot moves only |
| Balanced | 48 lines | 40 FPS | 64 KiB | whole-control layers disabled; small-object pulse allowed |
| Quality | 60 lines | 50 FPS | 128 KiB | large effects allowed only after budget check |

CLI buffer/FPS/heap values override the selected profile. CMake generates
`lv_conf.h` from `lv_conf.h.in`; `LVGL_MENU_BASE_HEAP_KIB` controls the built-in
TLSF pool and defaults to 64 KiB. After `lv_init`, an aligned RAII storage block
is added with `lv_mem_add_pool`. Its storage outlives `lv_deinit`, because theme
and cache allocations may remain in that pool after application widgets are
deleted.

Low/Balanced never set whole-control `transform_scale` or layered `opa`; visual
hierarchy comes from the independent focus frame, background, and label colors.
Quality estimates a layer as
`(width + 10) * min(height + 10, bufferLines) * 4 + 8 KiB` and compares that
against `lv_mem_monitor().free_biggest_size`; an insufficient block disables
large-object effects for the run. Continuous animation updates are limited to
the resolved refresh period.

The first frame is rendered synchronously and logs elapsed time plus LVGL total,
peak use, free bytes, largest contiguous free block, and fragmentation. Optional
runtime statistics add flush bytes/time and timer-handler time. LVGL warnings
are printed, compressed fonts are enabled, and an assertion logs then aborts
instead of entering the default infinite loop.

## Adding a page or control

Keep a new feature decision-complete across the pure model and LVGL view:

1. Add the page identity and its serializable-in-memory fields to
   `MenuPage`/`MenuSnapshot`.
2. Define Browse, Edit, Activate, and Back behavior in `MenuModel`; avoid LVGL
   types and calls here.
3. Add host tests for entry, focus wrapping or boundaries, edits, activation,
   and the two-stage Edit-to-Back path where applicable.
4. Add the Chinese home label and page builder in `MenuApplication`; place
   focusable controls in the page's LVGL group/event path in navigation order.
5. Retarget existing motion helpers instead of introducing page-local timing
   constants. Cancel old target animations before starting replacements.
6. Add every new Chinese character, ASCII symbol, or punctuation mark to the
   font subset command and regenerate both sizes only when each size uses it.
7. Rebuild host tests, the ARM Release target, and then exercise rapid/repeated
   input on the board.

Do not call LVGL from a `ButtonManager` callback. If a new gesture needs raw
timing, extend the pure button state machine and tests first, then map it in
`ThreeKeyLvglInput`.

## Font assets

The two checked-in LVGL C fonts are generated bitmap subsets, not runtime TTF
files:

- `lv_font_ui_cn_16`: body text and status values;
- `lv_font_ui_cn_20`: page/card titles and large values.

Their source is the locally installed `C:/Windows/Fonts/NotoSansSC-VF.ttf`.
Noto Sans SC is distributed under the **SIL Open Font License 1.1**. Retain the
OFL notice and attribution when redistributing or regenerating these derived
font resources; the in-tree notice is `src/assets/fonts/FONT_LICENSE_en.md`. The
generated source is deliberately committed so normal CMake builds do not need
Node.js, `lv_font_conv`, the TTF, or network access.

Use `lv_font_conv` from a temporary tooling environment; do not make it an
application build dependency. From the repository root, the checked-in files
were generated with the following commands (also preserved in each generated
file header):

```powershell
$symbols = "上下两个中事互交亮件保值停入关切动单压反吸呼回圆存实度开快态慢择按换播放数时暂最次步浏状环画确编节菜览认调轨辑近返进选速道释键长馈验高0123456789 -/SwitchmsPressReleaseClickDoubleClickLongPressUnknown"

npx lv_font_conv --size 16 --bpp 4 --format lvgl `
  --font C:/Windows/Fonts/NotoSansSC-VF.ttf `
  --symbols $symbols --force-fast-kern-format `
  --lv-font-name lv_font_ui_cn_16 `
  -o example_lvgl_st7789_menu/src/assets/fonts/lv_font_ui_cn_16.c

npx lv_font_conv --size 20 --bpp 4 --format lvgl `
  --font C:/Windows/Fonts/NotoSansSC-VF.ttf `
  --symbols $symbols --force-fast-kern-format `
  --lv-font-name lv_font_ui_cn_20 `
  -o example_lvgl_st7789_menu/src/assets/fonts/lv_font_ui_cn_20.c
```

The current symbol list contains only text that the UI can display, including
the English button-event names. When text changes, update `$symbols`, regenerate
both sizes, and review the generated header command. Do not use broad ASCII or
CJK ranges: they defeat the flash-size reason for subsetting. After regeneration,
verify all four pages on the target; a successful compile cannot detect a missing
glyph.

## Verification matrix

| Level | Required evidence |
| --- | --- |
| Pure button tests | debounce, bounce cancellation, long-press threshold, click/double-click ordering, polarity |
| Input bridge tests | FIFO queue, producer isolation, snapshots, repeat, direction conflict, Confirm exclusivity |
| Menu tests | focus wrap, page entry/return, edit retention, 0/100 clamp, switches, animation controls |
| Render-policy tests | auto thresholds, incomplete-data fallback, overrides, defaults, layer budget |
| Headless LVGL test | Low/40-line first frame, Home damage ≤ 90,000 pixels, rapid retarget settles, page motion completes, heap remains available |
| ARM artifact | ELF32 ARM hard-float, Release, static, no `INTERP` |
| Board | correct GPIO polarity/offsets, stable display, responsive rapid input, no animation backlog, fatal I/O exit |

The software input target is visible response within 50 ms after the configured
debounce interval. Board targets are: complete first frame within two seconds,
idle CPU below 30% on the constrained profile, ten minutes without heap
exhaustion, and the profile-selected 30/40/50 FPS target for damage-bounded
motion at 40 MHz SPI. These are
acceptance targets, not hard real-time guarantees.
