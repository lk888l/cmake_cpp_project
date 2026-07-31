# Linux / Buildroot 构建与运行

**中文** | [English](linux-buildroot_en.md)

## 1. 目标环境

当前默认目标是 32-bit ARM Linux hard-float，工具链前缀：

```text
arm-none-linux-gnueabihf-
```

默认运行参数：

| 参数 | 默认值 |
| --- | --- |
| I²C 设备 | `/dev/i2c-3` |
| 7-bit 地址 | `0x3C` |
| I²C 时钟请求 | 400000 Hz |
| 控制器 | SSD1306 |
| 分辨率 | 128×64 |
| 旋转 | 0° |
| 刷新周期 | 1000 ms |

Linux `i2c-dev` 的总线频率通常由设备树和控制器驱动决定。`clock_hz` 会传给 BSP 工厂，但当前 Linux 实现不在用户态修改控制器时钟；应在设备树或内核平台配置中确认实际频率。

## 2. 硬件连接

典型四针 I²C OLED：

| OLED | Linux 板端 | 说明 |
| --- | --- | --- |
| GND | GND | 必须共地 |
| VCC | 按模块规格连接 3.3 V | 不要仅根据排针标签假设可承受 5 V |
| SCL | I²C SCL | 需要上拉 |
| SDA | I²C SDA | 需要上拉 |

确认模块板上是否已经包含上拉电阻。多个模块并联时，总上拉阻值不能过低。Linux SoC 引脚必须配置为 I²C 复用功能，而不是普通 GPIO。

程序不控制独立 RESET 引脚。四针模块通常使用内部上电复位；如果产品模块引出了 RESET，应在平台启动阶段或扩展显示配置中实现硬件复位。

## 3. 交叉编译

从仓库根目录执行：

```powershell
cmake -S example_oled_portable -B example_oled_portable/build-arm `
  -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf `
  -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=OFF

cmake --build example_oled_portable/build-arm -j 8
```

CMake 会在 PATH 和 `C:/kk_software/toolchain/*arm-none-linux-gnueabihf*/bin` 中查找工具链。也可以显式指定：

```powershell
-DTOOLCHAIN_PATH=C:/path/to/toolchain/bin
```

有匹配目标 rootfs 的 sysroot 时：

```powershell
-DSYSROOT_PATH=C:/path/to/sysroot
```

默认 `USE_STATIC_LINKING=ON`。Release 构建完成后会执行 `strip --strip-unneeded`，产物位于：

```text
example_oled_portable/build-arm/src/example_oled_portable
```

## 4. 校验产物

```powershell
arm-none-linux-gnueabihf-readelf -h `
  example_oled_portable/build-arm/src/example_oled_portable

arm-none-linux-gnueabihf-readelf -l `
  example_oled_portable/build-arm/src/example_oled_portable

arm-none-linux-gnueabihf-size `
  example_oled_portable/build-arm/src/example_oled_portable
```

预期：

- `Class: ELF32`
- `Machine: ARM`
- hard-float EABI 标志
- 静态构建时没有 `INTERP` program header

如果文件明明存在，板端执行却报告 `not found`，首先检查 ELF 架构与动态解释器，不要只检查文件名。静态构建一般可以避开目标 rootfs 缺失 `/lib/ld-linux-armhf.so.3` 的问题。

## 5. Buildroot 集成方式

### 直接复制验证

```sh
scp example_oled_portable root@BOARD:/usr/bin/
ssh root@BOARD chmod +x /usr/bin/example_oled_portable
```

这是最适合首次硬件验证的方法。

### 作为 Buildroot 包

量产镜像建议建立自定义 package：

1. 将工程源码放进外部树或固定源码包。
2. package 的 configure 阶段使用 Buildroot 提供的交叉工具链。
3. install 阶段将程序放入 `/usr/bin`。
4. 确保内核启用 I²C 和 `CONFIG_I2C_CHARDEV`。
5. 设备树启用目标 I²C 控制器并正确配置 pinmux。

U8g2 已 vendored，不需要 Buildroot 构建阶段联网。

## 6. 板端准备

确认设备节点：

```sh
ls -l /dev/i2c-*
```

扫描地址：

```sh
i2cdetect -y 3
```

预期在 `3c` 或少数模块的 `3d` 位置看到响应。扫描操作本身会访问总线；若同一总线上有不允许 SMBus 探测的器件，应谨慎使用。

权限不足时，可临时以 root 运行。正式系统应配置 udev/mdev 权限或专用用户组，不建议长期给普通用户任意设备访问权限。

## 7. 命令行

```text
--bus PATH
--addr ADDRESS
--period-ms 1..60000
--rotation 0|90|180|270
--controller ssd1306|ssd1315
--demo dashboard|graphics|grayscale
--once
-h, --help
```

示例：

```sh
# 默认系统监控页面
./example_oled_portable

# SSD1315，地址 0x3D，旋转 180°
./example_oled_portable \
  --controller ssd1315 --addr 0x3d --rotation 180

# 绘制一次灰度测试图后退出
./example_oled_portable --demo grayscale --once
```

`--once` 仍会完成初始化、发送一帧，然后在退出路径调用省电模式。因此它适合验证 I²C 和页面生成，但屏幕会很快关闭；需要观察静态画面时，不要使用 `--once`，或后续增加“退出时不休眠”调试选项。

## 8. 演示页面

### `dashboard`

从 Linux 获取：

- `localtime_r()`：时间。
- `getifaddrs()`：第一个非 loopback IPv4 地址。
- `/proc/stat`：CPU 占用差分。
- `/proc/meminfo`：内存占用。

第一次 CPU 采样没有历史值，显示 0% 属于正常行为。

### `graphics`

用于确认裁剪、几何绘制、字体和中间灰度图案。若某些图形正常而文字异常，优先检查字体链接和 framebuffer，而不是 I²C 初始化。

### `grayscale`

显示 0～255 的多个 Bayer 覆盖等级。图案应静止；若整屏闪烁，通常是供电、接线、任务重复清屏或 I²C 错误，而不是设计中的时间灰度。

## 9. 退出行为

SIGINT 和 SIGTERM 会让主循环停止，然后调用：

```cpp
oled.setPowerSave(true);
```

因此正常退出后屏幕熄灭。强制断电、`SIGKILL` 或进程崩溃不会执行此清理路径。
