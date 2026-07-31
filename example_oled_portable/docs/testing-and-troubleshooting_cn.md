# 测试与故障排查

**中文** | [English](testing-and-troubleshooting_en.md)

## 1. 测试分层

OLED 问题通常来自四个不同层次，应分开验证：

1. 纯 framebuffer 算法：绘图、裁剪、字体、灰度。
2. U8g2 转换：初始化命令、控制字节和数据分包。
3. BSP/操作系统：设备节点、地址、权限、I²C ioctl/HAL。
4. 实体硬件：接线、上拉、供电、控制器型号和面板偏移。

不要在屏幕不亮时立刻修改绘图算法。先判断初始化是否成功、I²C 是否 ACK，以及 framebuffer 是否已经产生像素。

## 2. 本机单元测试

本机测试不构建 Linux executable，只构建 `oled_core` 和 Fake I²C：

```powershell
cmake -S example_oled_portable -B example_oled_portable/build-local `
  -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=local `
  -DBUILD_TESTING=ON

cmake --build example_oled_portable/build-local -j 8
ctest --test-dir example_oled_portable/build-local --output-on-failure
```

`oled_core_tests` 当前覆盖：

- SSD1306 初始化产生 I²C 事务。
- 每个 SSD13xx 数据包以 `0x00` 或 `0x40` 控制字节开头。
- 显示逻辑尺寸为 128×64，framebuffer 为 1024 B。
- BSP 写失败转换成 `transport_error`。
- 初始化失败不会置 `initialized_`。
- intensity 0、128、255 的确定性 Bayer 覆盖率。
- 越界线、矩形、圆和圆弧不会写坏 framebuffer。
- UTF-8 文本宽度和灰度文本产生非空像素。
- 90° 旋转后逻辑尺寸为 64×128。
- SSD1315 setup 至少能在 Fake BSP 上生成合法初始化事务。

这些测试不能证明实体面板型号、接线或电气时序正确。

## 3. 推荐新增测试

产品化时建议加入：

- 对三个演示页面保存 framebuffer golden/hash。
- 0～255 全部 intensity 的覆盖单调性。
- `drawGrayBitmap()` 的 stride、裁剪和空指针。
- 所有 `DisplayStatus` 与 `I2CStatus` 错误映射。
- 64-byte 聚合缓冲边界和人为 overflow。
- 旋转后的灰度文字图案一致性。
- 连续数万次 present 的内存和句柄泄漏检查。
- MCU 硬件在 100/400 kHz 下的压力测试。

## 4. 调试信息建议

初始化失败日志至少包含：

```text
platform
bus identifier
7-bit address
controller profile
rotation
I2CStatus
DisplayStatus
```

不要在普通日志中打印整个 framebuffer。需要图像诊断时，把 1024 B 保存为单独二进制或转换成 PBM 文件。

MCU 建议增加计数器：

- 初始化次数。
- 成功帧数。
- I²C timeout 次数。
- NACK/bus error 次数。
- 总线恢复次数。
- 最大 present 耗时。

## 5. 常见故障

### `/dev/i2c-3` 不存在

可能原因：

- 内核没有启用 `i2c-dev`。
- 设备树禁用了 I²C 控制器。
- 实际编号不是 3。
- pinmux 未配置。

检查：

```sh
ls /sys/class/i2c-dev
ls -l /dev/i2c-*
dmesg | grep -i i2c
```

不要仅通过 SoC 手册里的控制器序号推断 Linux `/dev/i2c-N` 编号。

### `not_found`

Linux BSP 在打开设备节点失败，或底层返回 `ENOENT/ENODEV/ENXIO` 时产生。先检查路径和内核设备，不是 OLED 绘图问题。

### `transport_error`

表示 I²C 写事务失败。常见原因：

- 地址 0x3C/0x3D 选择错误。
- SDA/SCL 接反或没有共地。
- 电压不匹配。
- 上拉缺失或过强。
- 总线被其他设备拉低。
- 屏幕掉电但进程仍在刷新。

使用示波器或逻辑分析仪确认 START、地址 ACK、控制字节和数据。仅看到 SCL 波形不能证明从机 ACK。

### 初始化成功但屏幕全黑

按顺序检查：

1. `--controller ssd1306` 与 `ssd1315` 是否匹配。
2. 是否在 `present()` 后立即调用了省电模式；`--once` 会快速退出并关屏。
3. contrast 是否过低。
4. 页面是否只画了 intensity 0。
5. 模块是否需要外部 RESET。
6. 控制器是否实际为 SH1106 或其他型号。

### 画面左右偏移或边缘缺列

通常是控制器/面板几何 profile 不匹配，例如 SH1106 有 132 列显存但只显示 128 列，部分模块需要列偏移。不要在业务页面统一给 X 加常数；应新增或选择正确 U8g2 display setup。

### 上下颠倒或镜像

先使用 `--rotation` 验证。若旋转后坐标正确但文字仍镜像，可能是错误控制器 setup 或模块扫描方向差异，需在 display profile 层处理。

### 文字乱码或空白

- 源码字符串必须是 UTF-8。
- 当前字体必须包含 glyph。
- `numeric` 字体不能显示普通字母。
- `drawText()` 的 Y 是 baseline，坐标太小可能把 glyph 画到屏幕外。
- 中文需要中文字体或定制子集。

### 灰度像棋盘格

这是空间抖动的预期特征。可通过以下方式改善观感：

- 只在较大填充区域使用中间 intensity。
- 关键文字和细线使用 255。
- 改用更高分辨率屏幕。
- 真正需要平滑灰度时换 SSD1327 等灰度控制器和对应渲染后端。

### 灰度图案闪烁

Bayer 图案本身不随帧变化。闪烁说明：

- 页面交替清屏与绘制但刷新节奏异常。
- 多任务无锁修改 framebuffer。
- 供电压降或接触不良。
- I²C 错误导致部分帧缺失。
- 业务每帧改变了 intensity 或图案坐标。

### `buffer_overflow`

U8g2 byte callback 的聚合缓冲为 64 B，当前 SSD1306/SSD1315 I²C CAD 数据块通常不会超过该值。若更换控制器或修改 CAD 分包方式后触发：

1. 记录单次 SEND 累计长度。
2. 确认 START/END 消息是否配对。
3. 合理增大 `transfer_buffer_`，同时检查 MCU 栈/RAM。
4. 不要简单丢弃溢出数据，否则会造成难以诊断的花屏。

### MCU 固件突然增大

优先查看 map 文件：

- 是否链接了大字体或完整中文字体。
- `drawArc()` 是否引入双精度 `sin/cos`。
- `fillCircle()` 是否引入 `sqrt`。
- 是否缺少 `-ffunction-sections/-fdata-sections/--gc-sections`。
- U8g2 字体是否位于可垃圾回收的独立 section。

不要因为 `u8g2_fonts.c` 很大就直接删除文件；应根据最终符号和 section 证据裁剪。

### MCU RAM 不足

当前最大明确缓冲是 1024 B framebuffer 加 1024 B 文字 scratch。优化顺序：

1. 不需要灰度文字时移除 scratch 路径。
2. 将 scratch 做成调用方提供的共享缓冲。
3. 改用 U8g2 page buffer 模式，以更多绘制次数换 RAM。
4. 减小任务栈，但必须先测量 high-water mark。

不要把 framebuffer 放到函数局部栈上。

## 6. 逻辑分析仪判定

典型 SSD13xx I²C 写事务：

```text
START
7-bit address + W
ACK
0x00 or 0x40
ACK
payload...
STOP
```

- `0x00` 后是命令和参数。
- `0x40` 后是 framebuffer 数据。

若控制字节和 payload 被拆成两个独立事务，SSD1306 不会按预期解释后一个事务；这也是 `I2CDevice::write()` 要求单次连续发送全部输入数据的原因。

## 7. 发布前验收

- Debug 和 Release 均可构建。
- 本机 CTest 全部通过。
- 交叉 ELF 架构、ABI 和静态/动态策略符合 rootfs。
- SSD1306 与 SSD1315 实物分别验证。
- 0° 和产品实际安装方向验证。
- 系统启动、进程重启、睡眠唤醒、掉电恢复验证。
- 24 小时刷新压力测试。
- I²C 与同总线传感器并发压力测试。
- 记录最终 Flash、RAM、任务栈和单帧耗时。
