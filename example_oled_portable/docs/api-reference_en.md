# Public API Reference

[中文](api-reference_cn.md) | **English**

## 1. Basic types

Display interfaces are in namespace `display`; I²C interfaces are in namespace `bsp`.

### `DisplayStatus`

| Value | Meaning | Typical cause |
| --- | --- | --- |
| `ok` | Operation succeeded. | — |
| `invalid_argument` | An argument or configuration is invalid. | `delay_ms == nullptr` |
| `not_initialized` | A hardware operation was called before initialization. | `present()` before `initialize()` |
| `buffer_overflow` | A U8g2 transaction exceeded the 64-byte aggregation buffer. | A changed CAD strategy or controller generated an oversized message. |
| `transport_error` | The BSP write failed. | Missing device, NACK, bus error, or permission failure. |

`toString(DisplayStatus)` returns a static string suitable for logs.

### `Rotation`

- `deg0`: native landscape, logical size 128×64.
- `deg90`: clockwise, logical size 64×128.
- `deg180`: inverted landscape, logical size 128×64.
- `deg270`: counterclockwise, logical size 64×128.

U8g2 performs the coordinate transform. Applications must query `width()` and `height()` rather than hard-code every page boundary.

### `Controller`

- `ssd1306`: selects U8g2 setup `ssd1306_i2c_128x64_noname_f`.
- `ssd1315`: selects U8g2 setup `ssd1315_i2c_128x64_noname_f`.

Panel dimensions do not uniquely identify the controller. Even when an SSD1315 module lights with an SSD1306 profile, record and select the real controller so initialization differences are not hidden.

### `OledDisplayConfig`

```cpp
struct OledDisplayConfig {
    Rotation rotation = Rotation::deg0;
    Controller controller = Controller::ssd1306;
    std::uint8_t contrast = 0xCF;
    void (*delay_ms)(std::uint32_t) = nullptr;
};
```

`contrast` is passed to U8g2/the controller and ranges from 0 to 255. It controls whole-panel hardware contrast, not per-pixel grayscale.

The platform must provide `delay_ms`. Linux uses `sleep_for`, STM32 may use `HAL_Delay`, and ESP32 may wrap an RTOS delay. The current contract expresses milliseconds only; see [mcu-porting_en.md](mcu-porting_en.md) for the microsecond limitation.

## 2. `OledDisplay`

### Construction

```cpp
OledDisplay(bsp::I2CDevice& device, OledDisplayConfig config = {});
```

Construction prepares the U8g2 context but does not communicate with hardware. The display neither copies nor owns `device`; the caller must keep it alive longer than the display.

A default `OledDisplayConfig` has no delay function, so calling `initialize()` with it returns `invalid_argument`. Production code must set `delay_ms`.

### `initialize()`

Initializes the controller, exits power-save mode, applies contrast, clears the framebuffer, and sends the black frame to the OLED. `isInitialized()` becomes `true` on success.

Do not call this from an interrupt. Initialization includes multiple I²C transactions and delays.

### `present()`

Sends the complete current framebuffer to the OLED. It returns `not_initialized` before successful initialization. The implementation does not retry automatically; after `transport_error`, the application chooses whether to retry, recover the bus, or reinitialize.

### `setPowerSave(bool enabled)`

Turns SSD1306/SSD1315 display output off or on. Use `true` during sleep or clean shutdown and `false` after wake. It does not release the framebuffer or discard rendered content.

### `setContrast(uint8_t contrast)`

Sets hardware contrast. Repeated contrast changes are not a grayscale-rendering technique and must not be performed per pixel or per row.

### State and dimensions

```cpp
bool isInitialized() const;
DisplayStatus lastStatus() const;
std::uint16_t width() const;
std::uint16_t height() const;
```

`lastStatus()` records the most recent error raised during a U8g2 callback. The direct return value of each operation remains the primary result.

### U8g2 and framebuffer access

```cpp
u8g2_t& nativeHandle();
const std::uint8_t* framebufferData() const;
std::size_t framebufferSize() const;
```

`nativeHandle()` is primarily used to construct `OledCanvas`, but it also permits advanced use of unwrapped U8g2 functions. Direct callers must preserve the shared framebuffer and draw-color state so later canvas operations remain predictable.

`framebufferData()` is a read-only diagnostic view. For the 128×64 full-buffer configuration, `framebufferSize()` is 1024 bytes. Do not retain the pointer beyond the `OledDisplay` lifetime.

## 3. `OledCanvas`

### Coordinates and intensity

- Origin `(0, 0)` is the logical top-left corner.
- X increases to the right and Y increases downward.
- Coordinates are signed `int`; out-of-range pixels are clipped.
- Intensity `0` is black and `255` is fully lit; intermediate values use Bayer spatial dithering.
- Intermediate intensity is a coverage pattern, not alpha blending.

### Clear

```cpp
void clear(std::uint8_t intensity = 0);
```

`clear(0)` clears the U8g2 framebuffer directly and is the fastest path. Intermediate and full-brightness clears generate the pattern pixel by pixel.

### Basic geometry

```cpp
void drawPixel(int x, int y, uint8_t intensity = 255);
void drawLine(int x0, int y0, int x1, int y1, uint8_t intensity = 255);
void drawRect(int x, int y, int width, int height, uint8_t intensity = 255);
void fillRect(int x, int y, int width, int height, uint8_t intensity = 255);
void drawCircle(int cx, int cy, int radius, uint8_t intensity = 255);
void fillCircle(int cx, int cy, int radius, uint8_t intensity = 255);
void drawTriangle(int x0, int y0, int x1, int y1,
                  int x2, int y2, uint8_t intensity = 255);
void drawArc(int cx, int cy, int radius,
             int start_degrees, int end_degrees,
             uint8_t intensity = 255);
```

Rectangles with non-positive width or height and circles with a negative radius draw nothing. Triangles currently support outlines only.

`drawArc()` uses screen coordinates: 0° points right and 90° points down. When the end angle is less than the start angle, 360° is added; the maximum span is 720°. The implementation uses `sin`, `cos`, and `lround`, which may pull a sizable math library into MCU firmware.

### Progress bar

```cpp
void drawProgressBar(int x, int y, int width, int height,
                     float percent, uint8_t intensity = 255);
```

`percent` is clamped to 0–100. The border is always fully lit, the completed interior uses the requested intensity, and the remaining interior is cleared. Nothing is drawn when width or height is less than 3.

### Bitmaps

```cpp
void drawBitmap(int x, int y, int width, int height,
                const uint8_t* xbm);
```

Input is XBM/LSB-first 1-bit data accepted by U8g2 `DrawXBM`. Keep the data in static read-only storage and provide at least `ceil(width / 8) * height` bytes.

```cpp
void drawGrayBitmap(int x, int y, int width, int height,
                    const uint8_t* gray8, size_t stride = 0);
```

Gray8 uses one 0–255 byte per pixel. A zero stride means tightly packed `width`-byte rows; an explicit stride must be at least `width`. Conversion dithers each pixel, making this suitable for small icons and test images—not continuous full-screen video conversion on a low-frequency MCU.

### Fonts and text

```cpp
enum class Font { small, medium, large, numeric };

void setFont(Font font);
int textWidth(const char* utf8) const;
void drawText(int x, int baseline_y, const char* utf8,
              uint8_t intensity = 255);
```

Built-in mappings:

| Enum | U8g2 font | Intended use |
| --- | --- | --- |
| `small` | `u8g2_font_5x7_tr` | Status bars and small labels |
| `medium` | `u8g2_font_6x10_tr` | Body text |
| `large` | `u8g2_font_helvB12_tr` | Titles |
| `numeric` | `u8g2_font_logisoso16_tn` | Large numeric values; numeric glyphs only |

`baseline_y` is the font baseline, not the top of its bounding box. Use the selected font's ascent/descent, or validate baseline placement on the target panel.

Strings must be null-terminated and remain valid for the call. UTF-8 is decoded, but rendering still depends on the selected font containing each glyph; the default fonts primarily cover ASCII. `textWidth(nullptr)` returns 0, and `drawText()` ignores null or empty strings.

Grayscale text first renders a glyph mask and then uses the 1024-byte `text_scratch_` buffer to filter only newly introduced foreground pixels. The background stays transparent and old content is not erased automatically.

## 4. I²C BSP

### `I2CDevice`

```cpp
virtual I2CStatus write(const uint8_t* data, size_t length) = 0;
virtual I2CStatus writeRead(const uint8_t* write_data, size_t write_length,
                            uint8_t* read_data, size_t read_length) = 0;
```

The OLED path currently uses only `write()`. Each call must be one continuous I²C write transaction and must not be split by a STOP. Input already contains the SSD13xx control byte:

- `0x00`: commands and parameters follow.
- `0x40`: display data follows.

The BSP must not insert another control byte. The device address is bound by `createDevice()` or the device constructor and is not part of the write payload.

### `I2CBus`

```cpp
virtual I2CStatus init() = 0;
virtual I2CStatus deinit() = 0;
virtual I2CDeviceResult createDevice(uint8_t address,
                                     uint32_t clock_hz) = 0;
```

`address` has 7-bit semantics and must be from 0 through `0x7F`. STM32 HAL often expects a left-shifted address; perform that conversion inside the STM32 BSP without changing the public contract.

## 5. Recommended application pattern

```cpp
auto result = bus.createDevice(0x3C, 400000);
if (!result) {
    // Handle I2CStatus.
}

display::OledDisplay oled(
    *result.device,
    {display::Rotation::deg0,
     display::Controller::ssd1306,
     0xCF,
     platformDelayMs});

if (oled.initialize() != display::DisplayStatus::ok) {
    // Record the initialization error.
}

display::OledCanvas canvas(oled.nativeHandle());
canvas.clear();
canvas.setFont(display::Font::medium);
canvas.drawText(0, 10, "Voltage");
canvas.drawProgressBar(0, 16, 100, 8, 72.5F, 160);

const auto status = oled.present();
```

Keep all page rendering in one display task. Other tasks should provide messages or immutable state snapshots instead of sharing the canvas directly.
