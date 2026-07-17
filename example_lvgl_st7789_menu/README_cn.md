# LVGL ST7789 三键菜单工程

[English README](README.md) | [架构与扩展说明](doc/architecture.md)

`example_lvgl_st7789_menu` 是一个面向嵌入式 Linux 的独立 C++23 示例工程，
目标硬件为 240 x 240 ST7789 彩屏和三个 GPIO 按键。工程继承
`example_lvgl_st7789` 的显示链路、`example_gpio_button` 的 Linux GPIO v2
按键链路，并增加线程安全输入桥、可主机测试的菜单状态机、中文字体资源和
手感明确的短动画。

彩屏使用只写 SPI，不依赖触摸屏。所有 `lv_*` 调用都留在主线程；GPIO 工作线程
只采集事件并写入互斥锁保护的队列，主循环消费队列后才更新菜单。

## 屏幕内容

首页是循环卡片轮播：选中卡片保持在中央，同时露出前后相邻卡片的一部分。四张
中文卡片对应四个详情页：

| 首页卡片 | 交互内容 |
| --- | --- |
| `圆环调节` | 编辑 0～100 的圆环数值，步长为 5，并实时播放数值动画 |
| `开关互动` | 切换呼吸动画与焦点高亮反馈，并观察控件动效 |
| `动画实验` | 选择慢/中/快三档速度，播放或暂停小范围呼吸/轨道动画 |
| `按键状态` | 查看三个按键的按下状态、按下次数、最近事件和持续时间 |

这些参数只是内存中的演示状态，程序重启后不会持久化。

## 按键映射

| 实体按键 | 短按行为 | 按住行为 |
| --- | --- | --- |
| 上 | 上一张卡片/上一个控件，或减小正在编辑的值 | 默认等待 400 ms 后，每 100 ms 连发一次 |
| 下 | 下一张卡片/下一个控件，或增大正在编辑的值 | 默认等待 400 ms 后，每 100 ms 连发一次 |
| 确认 | 进入页面、切换控件，或进入/结束编辑 | 长按返回；编辑状态下先退出编辑并保留当前值 |

上、下键按下时立即移动一步。两键同时按住时暂停方向连发；松开其中一个后，剩余
按键等待一个连发周期再恢复。确认键按下/松开直接驱动压感动画，Click 执行操作，
LongPress 返回；附加的 `DoubleClick` 事件不映射单独菜单命令。

首页长按确认不会退出程序，而是播放边界反馈。

## 工程结构

```text
example_lvgl_st7789_menu/
|-- CMakeLists.txt
|-- cmake/                       ARM Linux 工具链
|-- doc/architecture.md          所有权、数据流和扩展指南
|-- lv_conf.h.in                 自动生成的 LVGL 9 配置模板
`-- src/
    |-- bsp/                     Linux spidev 与输出 GPIO 适配层
    |-- hardware/                ST7789 控制器驱动
    |-- display/                 LVGL 局部缓冲显示桥
    |-- input/                   GPIO v2、手势状态机与按键事件队列
    |-- assets/fonts/            已生成的 16 px / 20 px 中文字体子集
    |-- app/                     纯菜单模型与 LVGL 视图
    |-- tests/                   主机端状态机和路由测试
    `-- main.cpp                 CLI、资源装配与主循环
```

本工程复制并整理参考实现，不跨工程链接 `example_lvgl_st7789` 或
`example_gpio_button` 的源码。

## 硬件与接线

### 前提条件

- 嵌入式 Linux 已提供 `/dev/spidevB.C`；三个按键输入需要 GPIO
  character-device v2；
- ST7789 模块使用 3.3 V SPI/GPIO 逻辑，常规分辨率为 240 x 240；
- 三个轻触按键接 GPIO 输入；默认低有效，需要开发板、设备树或外部电阻提供上拉；
- 运行用户有权打开选定的 SPI 和 GPIO 设备。

程序不负责配置 pinmux 和输入 bias，需在设备树或板级配置中预先完成。

LCD 输出 GPIO 适配器有意保留彩屏参考工程的实现；只有按键输入后端使用 GPIO
v2 边沿事件。

### 接线模板

| 外设引脚 | Linux 开发板信号 | CLI 参数/说明 |
| --- | --- | --- |
| LCD VCC | 3.3 V | 仍需核对具体模块数据手册 |
| LCD GND | GND | 必须共地 |
| LCD SCL/SCK | SPI SCLK | 硬件 SPI mode 0 |
| LCD SDA/MOSI | SPI MOSI | 本工程不使用 MISO |
| LCD CS | SPI CS | 必须与 `/dev/spidevB.C` 的片选号对应 |
| LCD DC/RS | 空闲 GPIO 输出 | `--gpiochip`、`--dc`，必填 |
| LCD RST | 空闲 GPIO 输出 | `--reset`，可选，低有效 |
| LCD BL | 安全的背光控制电路 | `--backlight`，可选，高有效 |
| 上键 | GPIO 输入与 GND 之间 | `--key-gpiochip`、`--key-up` |
| 下键 | GPIO 输入与 GND 之间 | `--key-down` |
| 确认键 | GPIO 输入与 GND 之间 | `--key-ok` |

若采用高有效按键，应按开发板安全的 3.3 V 输入电路接线，提供下拉，并加入
`--keys-active-high`。

> 有些 LCD 模块直接引出了背光 LED，不能在电流未知时直接用 SoC GPIO 供电。
> 请按模块要求增加电阻/三极管；若 BL 已安全接到 3.3 V，则不要传
> `--backlight`。

### GPIO line offset

`--dc`、`--reset`、`--backlight`、`--key-up`、`--key-down` 和
`--key-ok` 后的数字都是所选 gpiochip 内部的 **line offset**，不是排针物理
编号，也不是旧 sysfs 全局 GPIO 编号。在目标板上查询：

```sh
ls -l /dev/spidev* /dev/gpiochip*
gpioinfo
```

显示输出使用 `--gpiochip`，三个输入键使用 `--key-gpiochip`，二者可以是同一个
或不同的 gpiochip。每个物理 line 必须唯一，且不能已被 `gpio-keys`、LED
驱动、其他进程或其他内核驱动占用。

以下编号只用于解释参数，不能直接套用到其他开发板：

```text
/dev/gpiochip0 line 13 -> LCD DC
/dev/gpiochip0 line 12 -> LCD RST
/dev/gpiochip0 line 14 -> LCD BL
/dev/gpiochip1 line 25 -> 上键
/dev/gpiochip1 line 26 -> 下键
/dev/gpiochip1 line 27 -> 确认键
```

## 构建

### Windows 交叉编译 ARM Linux 程序

默认配置与仓库内同类工程一致：`arm-none-linux-gnueabihf`、Release、
hard-float、静态链接。对当前 GNU 交叉工具链，推荐使用 `MinGW Makefiles`。

```powershell
cd example_lvgl_st7789_menu
cmake --preset arm-release
cmake --build --preset arm-release
```

输出文件为 `build/arm-release/src/example_lvgl_st7789_menu`。上板必须使用全新
Release 目录；旧的 Debug LVGL 软件渲染在小型 SoC 上很容易占满 CPU。

首次在线配置会拉取固定的 LVGL v9.2.2。离线构建可直接指定已有且版本匹配的
LVGL 源码目录：

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
  -DLVGL_SOURCE_DIR=C:/path/to/lvgl `
  -DLVGL_MENU_BUILD_TESTS=OFF
```

若工具链不在 `PATH`，加入
`-DTOOLCHAIN_PATH=C:/path/to/arm-none-linux-gnueabihf/bin`；需要目标根文件系统
时加入 `-DSYSROOT_PATH=C:/path/to/sysroot`。

主要 CMake 选项：

| 选项 | 默认值 | 作用 |
| --- | --- | --- |
| `TARGET_PLATFORM` | `arm-none-linux-gnueabihf` | 选择 ARM Linux 或 `local` |
| `LVGL_MENU_BUILD_APP` | `ON` | 构建 Linux 彩屏程序和 LVGL |
| `LVGL_MENU_BUILD_TESTS` | `ON` | 构建可测试核心与测试程序 |
| `LVGL_MENU_BUILD_HEADLESS_TESTS` | `OFF` | 构建可选 LVGL 首帧测试 |
| `LVGL_MENU_BASE_HEAP_KIB` | `64` | 生成的 LVGL TLSF 基础池大小 |
| `LVGL_SOURCE_DIR` | 空 | 使用本地 LVGL，而不是拉取 v9.2.2 |
| `USE_STATIC_LINKING` | ARM 下为 `ON` | 添加静态链接选项 |
| `TOOLCHAIN_PATH` | 空 | 交叉工具链 `bin` 目录 |
| `SYSROOT_PATH` | 空 | 可选目标 sysroot |

### Windows 主机测试

关闭 App 后不会下载、配置或编译 LVGL，也不会编译 Linux 专用显示代码；输入和
菜单状态机仍可直接在 Windows 主机测试。

```powershell
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

在单独的本地构建中打开 `LVGL_MENU_BUILD_HEADLESS_TESTS=ON`，可编译 LVGL 并
验证 240×240、Low 档、8 行缓冲的首帧与页面切换。

ARM Release 构建完成后，可确认 ELF32 ARM hard-float、静态链接且不存在
`INTERP` program header：

```sh
file build/arm-release/src/example_lvgl_st7789_menu
arm-none-linux-gnueabihf-readelf -h build/arm-release/src/example_lvgl_st7789_menu
arm-none-linux-gnueabihf-readelf -l build/arm-release/src/example_lvgl_st7789_menu
```

## 上板运行

三个按键 line 和 LCD D/C 都是必填。RST、BL 已安全硬接时可以省略对应参数。

```sh
chmod +x example_lvgl_st7789_menu
./example_lvgl_st7789_menu \
  --spi /dev/spidev0.0 --spi-hz 40000000 \
  --gpiochip /dev/gpiochip0 --dc 13 --reset 12 --backlight 14 \
  --key-gpiochip /dev/gpiochip1 --key-up 25 --key-down 26 --key-ok 27
```

首次点屏建议先降到 10 MHz，确认颜色、方向、偏移和接线正确后再提高频率：

```sh
./example_lvgl_st7789_menu \
  --spi /dev/spidev0.0 --spi-hz 10000000 \
  --gpiochip /dev/gpiochip0 --dc 13 \
  --key-gpiochip /dev/gpiochip1 --key-up 25 --key-down 26 --key-ok 27
```

RV1103 或相近低资源平台的 `auto` 通常会选择 Low。首次验证可显式使用：

```sh
./example_lvgl_st7789_menu \
  --render-profile low --buffer-lines 8 --stats-interval-ms 2000 \
  --spi /dev/spidev0.0 --spi-hz 40000000 \
  --gpiochip /dev/gpiochip0 --dc 13 \
  --key-gpiochip /dev/gpiochip1 --key-up 25 --key-down 26 --key-ok 27
```

### 命令行参数

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| `--spi <path>` | `/dev/spidev0.0` | SPI 设备 |
| `--spi-hz <hz>` | `40000000` | SPI 时钟 |
| `--gpiochip <path>` | `/dev/gpiochip0` | LCD 控制 GPIO chip |
| `--dc <line>` | 必填 | LCD D/C line offset |
| `--reset <line>` | 省略 | LCD reset line offset |
| `--backlight <line>` | 省略 | LCD backlight line offset |
| `--width <px>` / `--height <px>` | `240` / `240` | 面板几何尺寸 |
| `--x-offset <px>` / `--y-offset <px>` | `0` / `0` | 控制器显存偏移 |
| `--rotation <deg>` | `0` | `0`、`90`、`180` 或 `270` |
| `--render-profile <name>` | `auto` | 选择 `auto`、`low`、`balanced` 或 `quality` |
| `--buffer-lines <n>` | 随档位 | 覆盖 LVGL 局部缓冲行数 |
| `--target-fps <n>` | 随档位 | 覆盖 1–60 FPS 刷新目标 |
| `--lvgl-extra-heap-kib <n>` | 随档位 | 覆盖附加 LVGL TLSF 池；`0` 表示不添加 |
| `--stats-interval-ms <ms>` | `0` | 周期输出刷新/定时器/内存统计；`0` 关闭 |
| `--shutdown-timeout-s <s>` | `3` | 第一次退出信号后的强制退出期限 |
| `--key-gpiochip <path>` | `/dev/gpiochip0` | 按键输入 GPIO chip |
| `--key-up <line>` | 必填 | 上键 line offset |
| `--key-down <line>` | 必填 | 下键 line offset |
| `--key-ok <line>` | 必填 | 确认键 line offset |
| `--keys-active-high` | 关闭 | 改用高有效按键 |
| `--debounce-ms <ms>` | `25` | 边沿消抖时间 |
| `--long-press-ms <ms>` | `600` | 确认键长按阈值 |
| `--double-click-ms <ms>` | `250` | 手势状态机双击窗口 |
| `--repeat-delay-ms <ms>` | `400` | 上/下键开始连发前的等待 |
| `--repeat-period-ms <ms>` | `100` | 上/下键连发周期 |
| `--help`、`-h` | - | 输出程序当前用法 |

程序会在启动阶段拒绝非法数字、缺少的必填 line、不支持的方向、重复物理 line 和
相互冲突的设备分配。

## 动画设计

动画时间集中在 `app::MotionTiming`：

- 焦点移动：160 ms，ease-out；
- 确认键按下：Low/Balanced 使用 2 px 位移和边框脉冲，Quality 缩放到 96%；
- 确认键松开：130 ms overshoot 回弹；
- 页面前进/返回：220 ms 定向滑动；
- 圆环/数值重定向：140 ms；
- 呼吸动画：900 ms 一个周期。

同一目标启动新动画前会删除旧动画，快速连按不会积压过时动画。`auto` 读取
`/proc/meminfo` 和在线 CPU 数；信息不完整时安全回退 Low。Low/Balanced 不对
整张卡片使用缩放或整体透明 layer；Quality 仅在 LVGL 最大连续空闲块满足预算时
启用。默认分别为 Low 8 行/20 FPS/+32 KiB、Balanced 12/25/+64 KiB、Quality
24/30/+128 KiB。

## 测试与板上验收

主机测试覆盖档位阈值、CLI 覆盖、layer 预算、消抖与抖动恢复、长按边界、双击事件顺序、有效电平、生产者/消费者
队列顺序、立即移动、连发时间、主线程停顿后不突发补帧、相反方向键、确认激活与
返回互斥、菜单循环、数值边界、Switch、编辑模式和页面返回。

还需在板上检查无法由主机测试证明的行为：

- 快速交替按上/下键，以及持续按住连发；
- 同时按住上/下，再只松开其中一个；
- 每一页的确认短按和长按；
- 反复进出页面后焦点不锁死、动画不积压；
- 按键持续按住时状态页计时正常；
- 所选 SPI 频率下画面稳定，无闪屏、无 Release 丢失；
- GPIO 工作线程或 SPI flush 失败后进程以非零状态退出。

## 故障排查

| 现象 | 检查项 |
| --- | --- |
| 缺少 `/dev/spidev*` 或 `/dev/gpiochip*` | 在设备树/内核中启用控制器、pinmux 和 character device |
| `Permission denied` | 先用 root 验证，再配置范围明确的 udev 规则或设备组 |
| `Device or resource busy` | 从 `gpio-keys`、LED 驱动、内核 consumer 或其他进程释放该 line |
| GPIO v2 ioctl 返回 `Invalid argument` | 确认内核支持 GPIO uAPI v2，且 line offset 未越界 |
| 按键完全无反应 | 核对 line offset、输入 pinmux、上下拉、共地和有效电平参数 |
| 按一次产生多次动作 | 检查干扰/接线并增大 `--debounce-ms`；Release + Click 本身是正常手势序列 |
| 画面偏移或裁切 | 修正 `--width`、`--height`、`--rotation`、`--x-offset`、`--y-offset` |
| 40 MHz 下画面不稳定 | 尝试 `--spi-hz 20000000` 或 `10000000`，再检查接线与信号完整性 |
| 只显示顶部一条且 CPU 100% | 用 `arm-release` 重新构建，运行时指定 `--render-profile low --buffer-lines 8`，并查看首帧 LVGL 内存日志 |
| Ctrl+C 不能及时退出 | 第一次信号启动清理与超时；再次 Ctrl+C 立即退出，也可调小 `--shutdown-timeout-s` |
| 中文显示缺字 | 确认两个字体 C 文件已通过字体 target 链接，且子集字符表包含新增文案 |

## 中文字体与许可证

工程提交的 16 px 正文和 20 px 标题资源，是从本机安装的
`NotoSansSC-VF.ttf` 生成的子集。Noto Sans SC 使用 SIL Open Font License
1.1。重新生成或分发字体数据时，应保留该许可证和归属说明。正常构建直接使用已
生成的 C 文件，不需要源 TTF、字体转换工具或网络。

工程内字体说明位于 `src/assets/fonts/FONT_LICENSE.md`。

具体再生成命令与字符集维护规则见
[doc/architecture.md](doc/architecture.md#font-assets)。
