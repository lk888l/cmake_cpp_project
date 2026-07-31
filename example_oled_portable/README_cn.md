# Portable OLED display example

**中文** | [English](README_en.md)

这是一个面向 Linux/Buildroot、ESP32 和 STM32 的轻量 OLED 图形工程。当前 BSP
实现运行于 Linux，目标屏幕为 0.96 寸 128x64 SSD1306/SSD1315 I2C OLED。

## 文档

- [文档中心](docs/README_cn.md)
- [架构与运行流程](docs/architecture_cn.md)
- [公共 API 参考](docs/api-reference_cn.md)
- [灰度与渲染](docs/grayscale-and-rendering_cn.md)
- [Linux / Buildroot 使用](docs/linux-buildroot_cn.md)
- [STM32F4 / ESP32 移植](docs/mcu-porting_cn.md)
- [测试与故障排查](docs/testing-and-troubleshooting_cn.md)

## 技术方案

- U8g2 C API 提供 SSD1306 初始化、全屏 framebuffer、UTF-8 字体和位图基础设施。
- `bsp::I2CBus` / `bsp::I2CDevice` 隔离平台通信；业务和显示代码不包含 Linux API。
- `OledCanvas` 提供文字、线、矩形、圆、三角形、圆弧、进度条和位图接口。
- SSD1306 是 1-bit 单色器件，中间灰度通过稳定的 4x4 Bayer 空间抖动模拟，
  不使用容易闪烁的时间 PWM。
- U8g2 源码固定在 `third_party/u8g2`，构建不需要联网。

## 目录

```text
example_oled_portable/
|- cmake/                 ARM Linux 交叉工具链
|- src/
|  |- app/                系统信息与三个演示页面
|  |- bsp/i2c/            可移植 I2C 接口和 Linux 实现
|  |- display/            OLED 驱动与跨平台绘图 API
|  `- tests/              假 I2C 与 framebuffer 测试
`- third_party/u8g2/      固定版本 U8g2
```

## ARM Linux 构建

在 Windows PowerShell 中：

```powershell
cmake -S example_oled_portable -B example_oled_portable/build-arm `
  -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf `
  -DCMAKE_BUILD_TYPE=Release
cmake --build example_oled_portable/build-arm -j
```

默认生成静态 ARM Linux ELF：

```text
example_oled_portable/build-arm/src/example_oled_portable
```

## 本机核心测试

本机测试不依赖 Linux I2C 头文件或实体屏幕：

```powershell
cmake -S example_oled_portable -B example_oled_portable/build-local `
  -G "MinGW Makefiles" -DTARGET_PLATFORM=local -DBUILD_TESTING=ON
cmake --build example_oled_portable/build-local -j
ctest --test-dir example_oled_portable/build-local --output-on-failure
```

## 板端运行

```sh
./example_oled_portable --bus /dev/i2c-3 --addr 0x3c --demo dashboard
./example_oled_portable --demo graphics
./example_oled_portable --demo grayscale
./example_oled_portable --controller ssd1315 --demo dashboard
```

其他选项：

```text
--period-ms 1000
--rotation 0|90|180|270
--controller ssd1306|ssd1315
--once
```

程序退出时会让 OLED 进入省电模式。若屏幕不响应，先用 `i2cdetect` 确认总线与
7-bit 地址，再检查模块是否实际兼容 SSD1306 初始化序列。

## 后续 MCU 移植

ESP32 或 STM32 只需实现 `bsp::I2CBus` 和 `bsp::I2CDevice`，并把平台延时函数传入
`OledDisplayConfig`。显示业务、U8g2 设备配置和 `OledCanvas` 不需要依赖 RTOS、HAL
或 ESP-IDF 类型。未来增加 64x48 或其他控制器时，应新增显示 profile，而不是在
业务页面中加入条件编译。
