# STM32F4 与 ESP32 移植指南

**中文** | [English](mcu-porting_en.md)

## 1. 结论与范围

显示核心适合移植到 STM32F4 和 ESP32。128×64 全缓冲加灰度文字 scratch 通常占约 2.3～3 KB RAM，ESP32 余量充足，大多数 STM32F4 也可接受。

当前仓库尚未提供 MCU BSP，不能把整个 Linux executable 直接加入 MCU。应复用：

- `src/display/oled_display.*`
- `src/display/oled_canvas.*`
- `src/bsp/i2c/bsp_i2c.*` 中的接口语义
- 所需 U8g2 核心、控制器与字体源码

不要移植：

- `linux_i2c_bus.*`
- `system_state.*`
- Linux `main.cpp`
- ARM Linux toolchain 文件

## 2. 移植前应先处理的限制

### 微秒延时

当前 `OledDisplayConfig` 只有 `delay_ms`。U8g2 的 `U8X8_MSG_DELAY_10MICRO` 被近似为 1 ms；对当前硬件 I²C SSD1306 初始化通常只造成额外延时，但不是严谨的 MCU 接口。

推荐扩展为：

```cpp
struct OledDisplayConfig {
    Rotation rotation;
    Controller controller;
    uint8_t contrast;
    void (*delay_ms)(uint32_t);
    void (*delay_us)(uint32_t);
};
```

然后分别处理毫秒与微秒消息。不要在中断上下文忙等很长时间。

### 浮点数学

`fillCircle()` 使用 `sqrt()`，`drawArc()` 使用双精度 `sin/cos/lround`，`drawProgressBar()` 使用 float。ESP32 一般可以接受；STM32F4 上双精度三角函数可能显著增加 Flash 和执行时间。

STM32 优化顺序：

1. 用中点圆/扫描线整数算法替换 `sqrt()`。
2. 用 U8g2 原生整数圆弧、查表或定点旋转替换 `sin/cos`。
3. 进度条改用整数千分比或百分比。
4. 查看 map 文件确认 `libm` 实际贡献。

### 堆与工厂

`OledDisplay` 和 `OledCanvas` 本身不动态分配内存。`I2CDeviceResult` 使用 `std::unique_ptr`，MCU 若禁止堆，可不使用 `I2CBus::createDevice()`，而是静态构造 BSP 对象：

```cpp
static Stm32I2CDevice i2c_device(hi2c1, 0x3C);
static display::OledDisplay oled(i2c_device, config);
```

更严格的产品可以把 `I2CDevice` 接口拆到不包含 `<memory>` 的独立头文件，让总线工厂仅在 Linux 使用。

## 3. 通用 MCU 集成步骤

1. 确认屏幕控制器、分辨率、I²C 地址和供电。
2. 配置硬件 I²C，建议先使用 400 kHz；不稳定时降到 100 kHz定位。
3. 实现 `bsp::I2CDevice::write()`。
4. 提供毫秒和建议的微秒延时回调。
5. 把 U8g2 与 `oled_core` 源码加入 MCU 构建系统。
6. 创建单一显示任务或明确的互斥保护。
7. 初始化后先绘制全亮边框和固定文字，确认坐标与方向。
8. 再启用业务页面和灰度图案。
9. 从 map 文件记录 RAM、Flash、字体和数学库成本。

## 4. STM32F4 BSP 示例

以下是接口形状示例，错误策略和超时需按产品规范调整：

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
        return bsp::I2CStatus::invalid_state; // OLED 当前不使用
    }

private:
    I2C_HandleTypeDef& handle_;
    uint8_t address_; // 保持 7-bit 语义
};
```

关键点：

- `HAL_I2C_Master_Transmit` 的设备地址参数通常需要 `address << 1`。
- 一次 `write()` 必须把整个输入缓冲作为一个事务发送，不能把首控制字节和数据拆成两个 STOP 分隔事务。
- 默认阻塞 HAL 适合单一显示任务。DMA 版本必须保证输入缓冲在传输完成前有效，并为异步完成重新设计 `I2CDevice` 契约，不能直接返回成功。
- FreeRTOS 下建议 OLED 仅由低优先级 UI 任务刷新，不在 ISR 中操作。

延时示例：

```cpp
void delayMs(uint32_t ms) { HAL_Delay(ms); }
```

微秒延时可使用 DWT cycle counter 或专用硬件定时器。不要假设所有 STM32F4 工程都已启用 DWT。

### STM32 构建选项

建议：

```text
-std=gnu++17
-ffunction-sections
-fdata-sections
-fno-exceptions        # 业务不使用异常时
-fno-rtti              # 无 dynamic_cast/typeid 时
-Wl,--gc-sections
```

`I2CDevice` 是多态接口，虚函数本身不要求 RTTI。关闭 RTTI 前仍应做完整链接验证。

## 5. ESP32 BSP 示例

ESP-IDF 5.x 新 I²C master driver 可将 `i2c_master_dev_handle_t` 封装为设备：

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

创建 device 时使用 7-bit `device_address = 0x3C`，总线速率设为 400 kHz。ESP-IDF 版本不同可能使用旧 `i2c_driver_install()` API；应基于工程已采用的 IDF 驱动体系实现，不要混用新旧驱动。

延时建议：

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

极短的 `delayMs(1)` 在低 tick rate 下可能被向下取整；包装函数应确保非零毫秒至少等待一个 tick，或对短延时使用微秒 API。

## 6. U8g2 源码精简

Linux 工程当前 glob 编译整个 `csrc/*.c`，便于增加控制器。MCU 不必编译所有设备驱动。至少需要保留：

- U8g2 framebuffer、字体、基础绘图核心。
- `u8g2_d_setup.c` 和对应 memory setup。
- `u8x8_byte.c`、`u8x8_cad.c`、显示/消息/GPIO基础文件。
- `u8x8_d_ssd1306_128x64_noname.c` 和/或 `u8x8_d_ssd1315_128x64_noname.c`。
- 实际字体定义。

精确最小列表应通过逐步链接验证生成，不能只复制上述文件名后假设依赖完整。若继续编译完整 U8g2，也必须启用 section GC，并检查最终 map；编译源很多不代表都会进入固件。

## 7. 任务模型

推荐单显示任务：

```text
传感器/通信任务 --> 状态快照或消息队列 --> UI任务 --> OledCanvas --> present()
```

优点：

- framebuffer 没有并发修改。
- I²C 超时不会阻塞控制环或 ISR。
- 页面刷新率容易限制。
- 可以统一处理总线恢复和显示重初始化。

若 OLED 与传感器共享 I²C，总线 BSP 还需要独立的总线互斥锁。显示级互斥不能替代总线级互斥。

## 8. MCU 验收清单

- 初始化返回 `ok`，屏幕先清黑。
- 四边框准确，无左右偏移或上下裁切。
- 旋转 0/90/180/270 的逻辑尺寸正确。
- SSD1306 和 SSD1315 profile 在对应模块上验证。
- 连续刷新 24 小时无 I²C 超时和花屏。
- 共享总线压力下无传感器采样异常。
- map 文件确认 framebuffer、scratch、任务栈、字体和 libm 占用。
- 低电压、复位和睡眠唤醒流程验证。
