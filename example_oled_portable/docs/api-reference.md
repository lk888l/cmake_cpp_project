# 公共 API 参考

## 1. 基本类型

所有显示接口位于 `display` 命名空间，I²C 接口位于 `bsp` 命名空间。

### `DisplayStatus`

| 值 | 含义 | 常见原因 |
| --- | --- | --- |
| `ok` | 操作成功 | — |
| `invalid_argument` | 参数或配置无效 | `delay_ms == nullptr` |
| `not_initialized` | 初始化前调用需要硬件的操作 | 初始化前 `present()` |
| `buffer_overflow` | U8g2 单次事务超过 64-byte 聚合缓冲 | 修改 U8g2 CAD 策略或扩展控制器后报文过长 |
| `transport_error` | BSP 写入失败 | 设备不存在、NACK、总线错误、权限问题 |

`toString(DisplayStatus)` 返回适合日志输出的静态字符串。

### `Rotation`

- `deg0`：原生横屏，逻辑尺寸 128×64。
- `deg90`：顺时针旋转，逻辑尺寸 64×128。
- `deg180`：横屏倒置，逻辑尺寸 128×64。
- `deg270`：逆时针旋转，逻辑尺寸 64×128。

旋转在 U8g2 坐标变换中完成；业务应通过 `width()` 和 `height()` 获取当前逻辑尺寸，不要硬编码所有页面边界。

### `Controller`

- `ssd1306`：调用 U8g2 的 `ssd1306_i2c_128x64_noname_f` 配置。
- `ssd1315`：调用 U8g2 的 `ssd1315_i2c_128x64_noname_f` 配置。

屏幕外观尺寸不能唯一确定控制器型号。若 SSD1315 模块使用 SSD1306 profile 也能点亮，仍建议在产品配置中记录真实控制器，避免初始化参数差异被掩盖。

### `OledDisplayConfig`

```cpp
struct OledDisplayConfig {
    Rotation rotation = Rotation::deg0;
    Controller controller = Controller::ssd1306;
    std::uint8_t contrast = 0xCF;
    void (*delay_ms)(std::uint32_t) = nullptr;
};
```

`contrast` 最终传给 U8g2/控制器，范围为 0～255。它是全屏硬件对比度，不是每像素灰度。

`delay_ms` 必须由平台提供。Linux 使用 `sleep_for`，STM32 可使用 `HAL_Delay`，ESP32 可使用 RTOS 延时包装。当前接口只表达毫秒延时；微秒精度限制见 [mcu-porting.md](mcu-porting.md)。

## 2. `OledDisplay`

### 构造

```cpp
OledDisplay(bsp::I2CDevice& device, OledDisplayConfig config = {});
```

构造只准备 U8g2 上下文，不与硬件通信。`device` 不被复制或接管，调用方必须保证其生命周期更长。

注意：默认构造的 `OledDisplayConfig` 没有延时函数，因此直接调用 `initialize()` 会返回 `invalid_argument`。实际代码应显式填写 `delay_ms`。

### `initialize()`

初始化控制器、退出省电、设置对比度、清空 framebuffer，并把黑屏帧发送到 OLED。成功后 `isInitialized()` 为 `true`。

不要在中断上下文中调用。初始化包括多个 I²C 事务和延时。

### `present()`

把当前完整 framebuffer 发送到 OLED。初始化前返回 `not_initialized`。当前实现没有自动重试；发生 `transport_error` 后由上层决定重试、复位总线或重新初始化。

### `setPowerSave(bool enabled)`

控制 SSD1306/SSD1315 显示开关。`true` 用于休眠或程序退出，`false` 用于唤醒。它不会释放 framebuffer，也不会丢弃已绘制内容。

### `setContrast(uint8_t contrast)`

设置硬件对比度。频繁改变对比度不是灰度绘制方法，也不应逐像素或逐行调用。

### 状态与尺寸

```cpp
bool isInitialized() const;
DisplayStatus lastStatus() const;
std::uint16_t width() const;
std::uint16_t height() const;
```

`lastStatus()` 记录最近一次 U8g2 回调阶段的错误。调用返回值仍是首选判断依据。

### U8g2 和 framebuffer 访问

```cpp
u8g2_t& nativeHandle();
const std::uint8_t* framebufferData() const;
std::size_t framebufferSize() const;
```

`nativeHandle()` 主要用于构造 `OledCanvas`，也允许高级业务调用 U8g2 未包装的功能。直接操作时必须遵守同一个 framebuffer 和 draw color 状态，避免与 `OledCanvas` 混用后留下不可预测状态。

`framebufferData()` 是只读诊断接口；128×64 全缓冲配置的 `framebufferSize()` 为 1024。不要保存该指针跨越 `OledDisplay` 生命周期。

## 3. `OledCanvas`

### 坐标与 intensity

- 原点 `(0, 0)` 在逻辑画面左上角。
- X 向右增加，Y 向下增加。
- 坐标参数使用有符号 `int`，越界像素被裁剪。
- `intensity = 0` 表示黑，`255` 表示全亮，中间值使用 Bayer 空间抖动。
- 中间 intensity 是覆盖图案，不是 alpha 混合。

### 清屏

```cpp
void clear(std::uint8_t intensity = 0);
```

`clear(0)` 直接清空 U8g2 framebuffer，速度最快。中间灰度和全亮清屏会逐像素生成图案。

### 基础图形

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

宽、高小于等于零的矩形不会绘制；负半径圆不会绘制。三角形当前只有轮廓，没有填充 API。

`drawArc()` 的角度以屏幕坐标计算：0° 指向右侧，90° 指向下方。若终止角小于起始角，函数会给终止角加 360°；最大绘制跨度限制为 720°。实现使用 `sin/cos/lround`，在 MCU 上可能引入较大的数学库，参见移植优化建议。

### 进度条

```cpp
void drawProgressBar(int x, int y, int width, int height,
                     float percent, uint8_t intensity = 255);
```

`percent` 自动限制在 0～100。外框始终全亮，内部已完成区域使用给定 intensity，剩余区域被清黑。宽或高小于 3 时不绘制。

### 位图

```cpp
void drawBitmap(int x, int y, int width, int height,
                const uint8_t* xbm);
```

输入必须是 U8g2 `DrawXBM` 接受的 XBM/LSB-first 1-bit 数据。位图数组应为静态只读数据，长度至少为 `ceil(width / 8) * height`。

```cpp
void drawGrayBitmap(int x, int y, int width, int height,
                    const uint8_t* gray8, size_t stride = 0);
```

灰度位图每像素一个字节，0～255。`stride == 0` 时按紧密排列的 `width` 字节处理；显式 stride 不能小于 width。该函数逐像素抖动，适合小图标和测试图，不适合在低频 MCU 上持续转换整屏视频帧。

### 字体与文字

```cpp
enum class Font { small, medium, large, numeric };

void setFont(Font font);
int textWidth(const char* utf8) const;
void drawText(int x, int baseline_y, const char* utf8,
              uint8_t intensity = 255);
```

内置映射：

| 枚举 | U8g2 字体 | 用途 |
| --- | --- | --- |
| `small` | `u8g2_font_5x7_tr` | 状态栏、小标签 |
| `medium` | `u8g2_font_6x10_tr` | 正文 |
| `large` | `u8g2_font_helvB12_tr` | 标题 |
| `numeric` | `u8g2_font_logisoso16_tn` | 大数字，仅包含数字类 glyph |

`baseline_y` 是字体基线，不是文字包围盒顶边。需要垂直布局时应结合选定字体的 ascent/descent，或先在目标屏幕上确认基线位置。

字符串必须以 `\0` 结尾并在调用期间有效。接口使用 UTF-8 解码，但能否显示某字符取决于当前字体是否包含对应 glyph；默认字体主要覆盖 ASCII。`textWidth(nullptr)` 返回 0，`drawText()` 对空指针和空字符串不操作。

灰度文字先绘制一份 glyph 蒙版，再使用 1024-byte `text_scratch_` 仅筛选新增前景像素。文字背景保持透明，不会自动清除旧内容。

## 4. I²C BSP

### `I2CDevice`

```cpp
virtual I2CStatus write(const uint8_t* data, size_t length) = 0;
virtual I2CStatus writeRead(const uint8_t* write_data, size_t write_length,
                            uint8_t* read_data, size_t read_length) = 0;
```

OLED 当前只使用 `write()`。一次调用必须对应一个连续、不可被 STOP 拆开的 I²C 写事务。输入数据已经包含 SSD13xx 控制字节：

- `0x00`：后续为命令/参数。
- `0x40`：后续为显示数据。

BSP 不应再次插入控制字节。地址由 `createDevice()` 或设备构造阶段绑定，不在 `write()` 数据中传递。

### `I2CBus`

```cpp
virtual I2CStatus init() = 0;
virtual I2CStatus deinit() = 0;
virtual I2CDeviceResult createDevice(uint8_t address,
                                     uint32_t clock_hz) = 0;
```

地址是 7-bit 地址，合法范围 0～`0x7F`。STM32 HAL 的传输函数通常要求左移一位，必须在 STM32 BSP 内部完成，不能改变公共接口语义。

## 5. 推荐业务使用模式

```cpp
auto result = bus.createDevice(0x3C, 400000);
if (!result) {
    // 处理 I2CStatus
}

display::OledDisplay oled(
    *result.device,
    {display::Rotation::deg0,
     display::Controller::ssd1306,
     0xCF,
     platformDelayMs});

if (oled.initialize() != display::DisplayStatus::ok) {
    // 记录初始化错误
}

display::OledCanvas canvas(oled.nativeHandle());
canvas.clear();
canvas.setFont(display::Font::medium);
canvas.drawText(0, 10, "Voltage");
canvas.drawProgressBar(0, 16, 100, 8, 72.5F, 160);

const auto status = oled.present();
```

建议把所有页面绘制集中在一个显示任务中，由其他任务通过消息或状态快照提供数据，避免多个调用者直接共享 canvas。

