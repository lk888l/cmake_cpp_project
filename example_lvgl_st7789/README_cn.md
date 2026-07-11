# example_lvgl_st7789

[English README](README.md)

这是一个运行在 Linux 用户态的 LVGL 9.2.2 示例，用于驱动 1.54 英寸、
240x240 分辨率的 ST7789 SPI LCD。工程沿用 `example_icm42688` 的交叉编译
配置，并将 Linux 设备访问、LCD 控制器、LVGL 显示适配和应用界面分层。

## 软件分层

```text
App/lvgl_demo_app
        |
Display/LvglSt7789Display       LVGL flush 回调、RGB565 字节序转换
        |
HardWare/St7789                ST7789 初始化、显示窗口设置
        |
Peripheral/SpiBus + OutputPin  BSP 抽象接口
        |
Linux spidev + GPIO chardev    Linux 平台实现
```

ST7789 驱动只依赖 `bsp::SpiBus` 和 `bsp::OutputPin`。以后移植到其他平台时，
可以替换底层 BSP，而不需要修改 ST7789 和 LVGL 适配层。

## LVGL 例程显示内容

初始化成功后，240x240 屏幕上应该显示：

- 深蓝黑色背景；
- 屏幕上方绿色的 `LVGL + ST7789` 标题；
- 屏幕中央浅色的 `Linux SPI BSP` 状态文字；
- 屏幕下方蓝色的 `Test` 按钮。

界面由 `src/App/lvgl_demo_app.cpp` 中的 `app::createDemoUi()` 创建。
`Test` 按钮已经注册 `LV_EVENT_CLICKED` 回调，点击后会把中央文字改成
`LVGL is running`。但是当前工程还没有注册触摸屏、鼠标、旋钮或按键等 LVGL
输入设备，因此上板后暂时无法操作这个按钮；需要增加输入 BSP 后点击回调才会执行。

程序成功启动后默认不会在终端持续打印信息。没有 error 且进程一直运行，表示程序
已经进入 LVGL 主循环，但不能据此证明实际 SPI/GPIO 接线正确。

## LVGL 的使用方法

### 初始化与调用流程

`src/main.cpp` 中的简化初始化顺序如下，其中 `panel_config` 表示由命令行参数
生成的屏幕尺寸、偏移和旋转配置：

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

运行时的数据流如下：

```text
LVGL 控件变化或屏幕刷新
           |
           v
LvglSt7789Display::flushCallback(刷新区域, RGB565 像素)
           |
           v
将每个 RGB565 像素转换为 ST7789 需要的线缆字节序
           |
           v
St7789::setAddressWindow() + writePixelBytes()
           |
           v
LinuxSpiBus -> /dev/spidevB.C -> LCD
```

`LvglSt7789Display::init()` 完成 LVGL 显示端口注册：

1. 调用 `lv_display_create(width, height)` 创建显示设备。
2. 选择 `LV_COLOR_FORMAT_RGB565` 像素格式。
3. 使用 `lv_display_set_flush_cb()` 注册 `flushCallback()`。
4. 使用 `lv_display_set_buffers()` 注册局部刷新缓冲区。
5. flush 回调把更新区域发送给 ST7789，最后调用
   `lv_display_flush_ready()`，通知 LVGL 可以重新使用缓冲区。

默认局部缓冲区高度为 24 行。240 像素宽、RGB565 格式下，LVGL 绘图缓冲区为
`240 * 24 * 2 = 11,520` 字节。适配层还会准备一个同样大小的发送缓冲区用于
字节序转换，因此两个缓冲区通常约占 23 KB。可以通过 `--buffer-lines` 调整内存
占用和单次 SPI 刷新数据量。

### LVGL tick 与主循环

当前 `lv_conf.h` 配置为 `LV_USE_OS LV_OS_NONE`，因此应用程序需要提供时间基准，
并在单一线程中运行 LVGL：

```cpp
while (true) {
    lv_tick_inc(elapsed_ms);
    uint32_t wait_ms = lv_timer_handler();
    std::this_thread::sleep_for(...);
}
```

`lv_tick_inc()` 推进 LVGL 的时间，用于动画和定时器；`lv_timer_handler()` 处理
控件更新、无效区域绘制，并最终调用显示 flush 回调。未增加锁机制前，应当让所有
LVGL API 调用都发生在这个线程中，不要从任意工作线程并发修改 LVGL 对象。

### 添加自己的界面

修改 `src/App/lvgl_demo_app.cpp`，并尽量不要在该文件中直接访问 SPI/GPIO。
控件必须在 `lv_init()` 和显示设备初始化完成后创建。例如：

```cpp
void createDemoUi()
{
    lv_obj_t* screen = lv_screen_active();

    lv_obj_t* label = lv_label_create(screen);
    lv_label_set_text(label, "Hello LVGL");
    lv_obj_center(label);
}
```

如果以后需要修改控件，可以保存其 `lv_obj_t*`，把它作为回调 user data 传递，
或者放在应用层自己的 UI 结构体中。正常 UI 代码不需要直接调用 SPI 或 ST7789；
只需要修改或刷新 LVGL 对象，已经注册的 flush 回调会自动完成屏幕传输。

LVGL 编译选项位于 `lv_conf.h`。当前配置使用 16 位颜色、LVGL 内置内存分配器、
无操作系统适配层、warning 级别日志，以及标题使用的 Montserrat 20 字体。

## 接线方法

### 接线前须知

- 关闭开发板电源后再连接 LCD。
- 本工程按 **3.3 V SPI/GPIO 电平**设计，不要把 5 V 逻辑信号直接连接到
  ST7789。
- 这类 SPI 屏幕上的 `SCL` 表示 SPI 时钟，`SDA` 表示 SPI MOSI，不是 I2C。
- 本工程只向屏幕写数据，不使用 MISO。

### 信号对应关系

| LCD 引脚 | Linux 开发板信号 | 说明 |
| --- | --- | --- |
| VCC | 3.3 V | 除非屏幕模块资料明确允许其他电压，否则使用 3.3 V |
| GND | GND | 屏幕和开发板必须共地 |
| SCL/SCK/CLK | SPI SCLK | 硬件 SPI 时钟，SPI mode 0 |
| SDA/MOSI/DIN | SPI MOSI | 硬件 SPI 发送数据 |
| CS | SPI CS0 或 CS1 | 必须与 `/dev/spidevB.C` 的片选号一致 |
| DC/RS/A0 | 空闲 GPIO 输出 | 必须连接，用于选择命令或像素数据 |
| RES/RST | 空闲 GPIO 输出 | 低电平复位，首次调试建议连接 |
| BL/LED | 背光控制 | 程序按高电平点亮处理，注意下面的背光警告 |

通用接线模板如下：

```text
ST7789 LCD                         Linux 开发板
----------                         ------------
VCC       -----------------------> 3V3
GND       -----------------------> GND
SCL/SCK   -----------------------> SPIx_SCLK
SDA/MOSI  -----------------------> SPIx_MOSI
CS        -----------------------> SPIx_CS0       -> /dev/spidevB.0
DC        -----------------------> 空闲 GPIO      -> --dc <line-offset>
RST       -----------------------> 空闲 GPIO      -> --reset <line-offset>
BL        -----------------------> 背光控制       -> --backlight <line-offset>
```

`B` 是 Linux SPI 总线编号，小数点后的数字是片选号。例如，
`/dev/spidev0.0` 表示 SPI0/CS0，`/dev/spidev1.1` 表示 SPI1/CS1。不同开发板
的排针编号和复用关系不一样，必须根据实际开发板原理图和当前设备树确认。

> **背光注意事项：**有些 LCD 模块自带背光限流电阻或三极管，有些模块则
> 直接引出 LED。不要用 SoC GPIO 直接驱动未知电流的背光。如果模块资料不明确，
> 应按资料增加限流电阻或三极管。若确认 BL 可以安全接 3.3 V，则可将 BL 固定
> 接到 3.3 V，运行程序时省略 `--backlight`。

### 查找 Linux 设备和 GPIO line offset

开发板设备树必须已经启用 SPI 控制器、对应 pinmux 和 `spidev` 节点。上板后执行：

```sh
ls -l /dev/spidev* /dev/gpiochip*
gpioinfo
```

为 DC、RST 和 BL 分别选择空闲 GPIO，并记录 gpiochip 路径及其 line offset。
当前程序只有一个 `--gpiochip` 参数，因此这三个控制信号通常应选择同一个
gpiochip 下的 GPIO。

本工程使用 Linux GPIO character-device ABI。`--dc`、`--reset` 和
`--backlight` 后面的数字是对应 gpiochip 内部的 **line offset**，不是开发板
物理排针号，也不是旧 sysfs 接口的全局 GPIO 编号。

下面的数字仅用于演示，**不是所有开发板通用的引脚编号**：

```text
/dev/gpiochip0 line 13 -> LCD DC
/dev/gpiochip0 line 12 -> LCD RST
/dev/gpiochip0 line 14 -> LCD BL
```

## Windows 下交叉编译

默认配置与 `example_icm42688` 一致：使用 `arm-none-linux-gnueabihf`、
Release、hard-float 和静态链接。第一次配置时会把固定版本 LVGL v9.2.2
下载到 build 目录。

```powershell
cd example_lvgl_st7789
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build -j 8
```

如果 CMake 无法自动找到编译器，可指定工具链的 `bin` 目录：

```powershell
cmake -G "MinGW Makefiles" -S . -B build `
  -DTOOLCHAIN_PATH=C:/path/to/arm-none-linux-gnueabihf/bin
cmake --build build -j 8
```

离线编译时，可以提前准备 LVGL 源码并指定路径：

```powershell
cmake -G "MinGW Makefiles" -S . -B build `
  -DLVGL_SOURCE_DIR=C:/path/to/lvgl
```

生成的程序位于 `build/src/example_lvgl_st7789`。

## 开发板运行准备

内核必须提供 SPI 和 GPIO character device。运行前检查：

```sh
ls -l /dev/spidev* /dev/gpiochip*
gpioinfo
```

程序需要打开这些设备的权限。首次调试可以使用 root；正式部署建议设置合适的
udev 规则或设备用户组。

下面演示使用 `/dev/spidev0.0`，DC 使用 gpiochip0 line 13，RST 使用 line 12，
BL 使用 line 14。请把示例 line offset 替换为实际开发板的值：

```sh
chmod +x example_lvgl_st7789
./example_lvgl_st7789 \
  --spi /dev/spidev0.0 \
  --spi-hz 40000000 \
  --gpiochip /dev/gpiochip0 \
  --dc 13 --reset 12 --backlight 14
```

只有 DC 是必需参数。如果 RST 或 BL 已经在硬件上固定连接，可以省略对应参数：

```sh
./example_lvgl_st7789 --spi /dev/spidev0.0 --gpiochip /dev/gpiochip0 --dc 13
```

屏幕尺寸、旋转方向和偏移量都可以在运行时修改：

```sh
./example_lvgl_st7789 --dc 13 \
  --width 240 --height 240 \
  --rotation 90 --x-offset 0 --y-offset 0 \
  --buffer-lines 24
```

执行 `--help` 可查看所有参数。如果图像颜色正确但位置偏移，请调整 X/Y offset。
如果 40 MHz 下屏幕显示不稳定，先降低到 20 MHz 或 10 MHz。首次通电建议从
10 MHz 开始：

```sh
./example_lvgl_st7789 --spi /dev/spidev0.0 --spi-hz 10000000 \
  --gpiochip /dev/gpiochip0 --dc 13 --reset 12 --backlight 14
```

## 当前功能范围

当前版本只实现显示输出。演示界面中包含一个 LVGL 按钮，但尚未注册触摸屏或按键
输入 BSP，因此在增加输入设备前，该按钮只用于显示效果验证。
