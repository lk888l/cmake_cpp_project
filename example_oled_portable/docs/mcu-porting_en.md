# STM32F4 and ESP32 Porting Guide

[中文](mcu-porting_cn.md) | **English**

## 1. Scope and conclusion

The display core is suitable for STM32F4 and ESP32. A 128×64 full framebuffer plus the grayscale-text scratch buffer normally consumes about 2.3–3 KiB of RAM. ESP32 has ample headroom, and most STM32F4 variants can accommodate it.

The repository does not yet provide a complete MCU BSP. Do not copy the Linux executable into an MCU project. Reuse:

- `src/display/oled_display.*`
- `src/display/oled_canvas.*`
- the interface semantics in `src/bsp/i2c/bsp_i2c.*`
- the required U8g2 core, controller, and font sources

Do not port:

- `linux_i2c_bus.*`
- `system_state.*`
- the Linux `main.cpp`
- the ARM Linux toolchain file

## 2. Constraints to address first

### Microsecond delays

`OledDisplayConfig` currently exposes only `delay_ms`. U8g2's `U8X8_MSG_DELAY_10MICRO` is approximated as 1 ms. That usually adds harmless delay during I²C SSD1306 initialization, but it is not a precise MCU contract.

Extend the configuration when exact timing is required:

```cpp
struct OledDisplayConfig {
    Rotation rotation;
    Controller controller;
    uint8_t contrast;
    void (*delay_ms)(uint32_t);
    void (*delay_us)(uint32_t);
};
```

Handle millisecond and microsecond messages separately. Never busy-wait for long intervals inside an interrupt context.

### Floating-point geometry

`fillCircle()` uses `sqrt()`, `drawArc()` uses double-precision `sin`, `cos`, and `lround`, and `drawProgressBar()` uses floating point. ESP32 generally tolerates this. On STM32F4, double-precision trigonometry can materially increase flash and execution time.

Recommended STM32 optimization order:

1. Replace `sqrt()` with an integer midpoint-circle or scan-line algorithm.
2. Replace `sin`/`cos` with U8g2 integer primitives, a lookup table, or fixed-point rotation.
3. Express progress as integer percent or per-mille.
4. Inspect the linker map to measure the actual `libm` contribution.

### Heap and factories

`OledDisplay` and `OledCanvas` do not allocate dynamically, but `I2CDeviceResult` uses `std::unique_ptr`. A heap-free MCU can bypass `I2CBus::createDevice()` and statically construct the BSP object:

```cpp
static Stm32I2CDevice i2c_device(hi2c1, 0x3C);
static display::OledDisplay oled(i2c_device, config);
```

For stricter products, move the `I2CDevice` interface into a header that does not include `<memory>` and keep the bus factory Linux-only.

## 3. Generic MCU integration

1. Confirm controller, resolution, I²C address, and supply requirements.
2. Configure hardware I²C at 400 kHz initially; reduce to 100 kHz while diagnosing instability.
3. Implement `bsp::I2CDevice::write()`.
4. Provide millisecond and, preferably, microsecond delay callbacks.
5. Add U8g2 and `oled_core` sources to the MCU build.
6. Use one display task or explicit mutual exclusion.
7. First draw a full-brightness border and fixed text to validate coordinates and orientation.
8. Then enable application pages and grayscale patterns.
9. Record RAM, flash, font, and math-library costs from the linker map.

## 4. STM32F4 BSP example

The following shows the intended interface shape. Adjust timeouts and recovery policy to the product requirements:

```cpp
class Stm32I2CDevice final : public bsp::I2CDevice {
public:
    Stm32I2CDevice(I2C_HandleTypeDef& handle, uint8_t address)
        : handle_(handle), address_(address) {}

    bsp::I2CStatus write(const uint8_t* data, size_t length) override
    {
        if (data == nullptr || length == 0 || length > UINT16_MAX) {
            return bsp::I2CStatus::invalid_argument;
        }

        const HAL_StatusTypeDef result = HAL_I2C_Master_Transmit(
            &handle_,
            static_cast<uint16_t>(address_) << 1,
            const_cast<uint8_t*>(data),
            static_cast<uint16_t>(length),
            100);

        if (result == HAL_OK) return bsp::I2CStatus::ok;
        if (result == HAL_TIMEOUT) return bsp::I2CStatus::timeout;
        return bsp::I2CStatus::io_error;
    }

    bsp::I2CStatus writeRead(...) override
    {
        return bsp::I2CStatus::invalid_state; // Not used by the OLED path.
    }

private:
    I2C_HandleTypeDef& handle_;
    uint8_t address_; // Preserve 7-bit semantics.
};
```

Important details:

- `HAL_I2C_Master_Transmit` normally expects `address << 1`.
- One `write()` call must send the entire input buffer as one transaction. Do not split the control byte and data with a STOP condition.
- A blocking HAL implementation is appropriate for a single display task. A DMA implementation must retain the input buffer until completion and redesign the `I2CDevice` contract for asynchronous completion; it must not return success immediately.
- Under FreeRTOS, update the OLED from a low-priority UI task, never from an ISR.

Millisecond delay:

```cpp
void delayMs(uint32_t ms) { HAL_Delay(ms); }
```

Use the DWT cycle counter or a hardware timer for microsecond delays. Do not assume that every STM32F4 project enables DWT.

Recommended build options:

```text
-std=gnu++17
-ffunction-sections
-fdata-sections
-fno-exceptions
-fno-rtti
-Wl,--gc-sections
```

Disable exceptions only when the application does not use them. `I2CDevice` is polymorphic but does not itself require RTTI; still validate the complete link after disabling RTTI.

## 5. ESP32 BSP example

With the ESP-IDF 5.x I²C master driver, wrap an `i2c_master_dev_handle_t`:

```cpp
class EspI2CDevice final : public bsp::I2CDevice {
public:
    explicit EspI2CDevice(i2c_master_dev_handle_t device)
        : device_(device) {}

    bsp::I2CStatus write(const uint8_t* data, size_t length) override
    {
        if (data == nullptr || length == 0) {
            return bsp::I2CStatus::invalid_argument;
        }
        const esp_err_t result =
            i2c_master_transmit(device_, data, length, 100);
        if (result == ESP_OK) return bsp::I2CStatus::ok;
        if (result == ESP_ERR_TIMEOUT) return bsp::I2CStatus::timeout;
        return bsp::I2CStatus::io_error;
    }

    bsp::I2CStatus writeRead(...) override
    {
        return bsp::I2CStatus::invalid_state;
    }

private:
    i2c_master_dev_handle_t device_;
};
```

Create the device with the 7-bit address `0x3C` and a 400 kHz bus speed. Older ESP-IDF releases use the legacy `i2c_driver_install()` API. Implement against one driver generation; do not mix legacy and current APIs.

Recommended delays:

```cpp
void delayMs(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void delayUs(uint32_t us)
{
    esp_rom_delay_us(us);
}
```

At a low RTOS tick rate, a very short `delayMs(1)` can round down. Ensure a nonzero millisecond delay waits at least one tick, or route short delays through the microsecond API.

## 6. Trimming U8g2

The Linux build currently compiles all `csrc/*.c` for easy controller expansion. An MCU need not compile every device driver. At minimum, retain:

- U8g2 framebuffer, font, and drawing cores;
- `u8g2_d_setup.c` and the matching memory setup;
- `u8x8_byte.c`, `u8x8_cad.c`, and required display-message/GPIO foundations;
- `u8x8_d_ssd1306_128x64_noname.c` and/or `u8x8_d_ssd1315_128x64_noname.c`;
- the font definitions actually used.

Derive the exact minimum list incrementally through successful links. If the complete U8g2 source set remains in the build, enable section garbage collection and inspect the final map. Compiled source is not necessarily retained firmware.

## 7. Task model

Use a single display owner:

```text
sensor/communication tasks
        -> state snapshot or message queue
        -> UI task
        -> OledCanvas
        -> present()
```

This prevents concurrent framebuffer updates, keeps I²C timeouts out of control loops and ISRs, bounds the refresh rate, and centralizes bus recovery and display reinitialization.

If the OLED shares I²C with sensors, the BSP also needs a bus-level mutex. A display-level lock does not replace bus arbitration.

## 8. MCU acceptance checklist

- Initialization returns `ok`, and the screen first clears to black.
- All four borders are accurate with no horizontal offset or vertical clipping.
- Logical dimensions are correct at rotations 0, 90, 180, and 270 degrees.
- SSD1306 and SSD1315 profiles are tested on their matching modules.
- A 24-hour continuous refresh has no I²C timeout or display corruption.
- Sensor sampling remains correct under shared-bus stress.
- The linker map confirms framebuffer, scratch, task stack, fonts, and `libm` usage.
- Brownout, reset, sleep, and wake behavior is validated.
