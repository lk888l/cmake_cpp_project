# OLED 驱动文档中心

**中文** | [English](README_en.md)

本文档集描述 `example_oled_portable` 的现有实现、使用方式和移植边界。除“移植建议”章节外，内容均由当前源码推导；Linux 构建和核心单元测试已经在开发环境验证，实体 OLED 和 MCU 板端行为仍需在对应硬件上确认。

## 阅读路线

| 文档 | 内容 | 适合读者 |
| --- | --- | --- |
| [architecture_cn.md](architecture_cn.md) | 分层结构、对象关系、初始化和刷新数据流 | 首次理解工程、架构评审 |
| [api-reference_cn.md](api-reference_cn.md) | `OledDisplay`、`OledCanvas`、I²C BSP 的完整接口说明 | 业务页面开发者 |
| [grayscale-and-rendering_cn.md](grayscale-and-rendering_cn.md) | framebuffer、坐标、字体、位图和 Bayer 伪灰度原理 | 图形效果和性能调试 |
| [linux-buildroot_cn.md](linux-buildroot_cn.md) | 构建、部署、命令行、I²C 接线与运行 | Linux/Buildroot 集成 |
| [mcu-porting_cn.md](mcu-porting_cn.md) | STM32F4、ESP32 移植步骤和资源优化 | MCU 驱动开发者 |
| [testing-and-troubleshooting_cn.md](testing-and-troubleshooting_cn.md) | 测试覆盖、诊断顺序和常见故障 | 测试、现场调试 |

## 当前能力边界

- 目标显示器：128×64 SSD1306 或 SSD1315，I²C 接口。
- 当前可运行 BSP：Linux `/dev/i2c-*`。
- 图形核心：U8g2 C API，全屏 1-bit framebuffer。
- 图形业务层：像素、直线、矩形、圆、三角形、圆弧、进度条、XBM、8-bit 灰度位图和 UTF-8 文本。
- 灰度：4×4 Bayer 空间抖动，不是 OLED 像素的真实多级亮度。
- 刷新：`present()` 每次发送完整 framebuffer，目前没有 dirty rectangle。
- MCU：接口已经隔离平台依赖，但仓库中尚未包含 STM32 HAL 或 ESP-IDF BSP 实现。

## 关键源码入口

- `src/display/oled_display.hpp`：显示设备、U8g2 回调和生命周期。
- `src/display/oled_canvas.hpp`：业务绘图 API。
- `src/bsp/i2c/bsp_i2c.hpp`：跨平台 I²C 契约。
- `src/app/oled_demo_app.cpp`：系统监控、图形、灰度演示。
- `src/main.cpp`：Linux 参数解析、对象装配与主循环。
- `src/tests/oled_core_tests.cpp`：假 I²C 与 framebuffer 测试。
