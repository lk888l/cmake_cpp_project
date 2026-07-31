# 架构与扩展指南

**中文** | [English](architecture_en.md)

本文记录 `example_lvgl_st7789_menu` 的运行时所有权边界和安全扩展方式。若未来
有意修改接口，以源码为最终依据。

## 设计目标

- 在 Linux 用户空间驱动 240x240 ST7789 和三个 GPIO 按键。
- GPIO 工作线程与 LVGL 解耦，所有 LVGL 调用只发生在一个所有者线程。
- 按键时序、输入路由和菜单导航无需 Linux 设备或 LVGL 即可测试。
- 在 SPI 局部缓冲显示上提供短小、可替换且响应迅速的动画。
- 明确分隔 BSP、硬件、显示、输入和应用层。

当前版本不包含触摸输入、运行时引脚重配置、持久化、PWM 亮度或动态页面注册。

## 目标与所有权

```text
example_lvgl_st7789_menu（main/CLI/生命周期）
  |-- menu_app ---------> menu_ui + menu_ui_fonts + LVGL + menu_input_core
  |-- menu_ui ----------> 重新定向时替换的 LVGL 动画原语
  |-- menu_runtime_core -> 资源探测 + 纯 RenderPolicy
  |-- menu_lvgl_runtime -> 附加 TLSF 内存池存储
  |-- menu_input_linux -> menu_input_core + Threads
  |-- menu_display -----> menu_st7789 + LVGL
  |-- menu_st7789 ------> menu_bsp
  `-- LVGL

主机测试
  |-- 纯逻辑测试 -------> menu_input_core + menu_runtime_core + MenuModel
  `-- 可选无头测试 -----> menu_app + LVGL，不使用 Linux 显示设备
```

| 层 | 主要职责 | 不得拥有 |
|---|---|---|
| `bsp` | Linux spidev 写入、GPIO 输出生命周期、状态转换 | LVGL 控件或菜单策略 |
| `hardware` | ST7789 初始化、方向、地址窗口、像素传输、背光命令 | BSP 接口之外的 Linux 文件描述符 |
| `display` | LVGL 显示创建、RGB565 局部缓冲、flush 回调与故障状态 | 菜单导航或 GPIO 输入 |
| `input` 核心 | 去抖/手势、线程安全原始事件队列、连发策略、遥测快照 | LVGL 调用 |
| `input` Linux | GPIO v2、`epoll`、`timerfd`、`eventfd`、工作线程生命周期 | 菜单/页面决策 |
| `app` 模型 | 页面、焦点、编辑、数值和开关的纯状态迁移 | 线程、Linux 设备、LVGL |
| `ui` | 可复用 LVGL 动画生命周期和统一时序 | 菜单状态或硬件访问 |
| `app` 表现层 | LVGL 对象树、样式、焦点/按下/页面/数值渲染 | GPIO 读取或回调线程工作 |
| `runtime` | 资源探测、渲染策略、附加 LVGL 内存池生命周期 | 芯片型号或控件决策 |
| `main` | 解析/校验 CLI、构造资源、桥接回调、运行和停止循环 | 控件特定业务规则 |

`LVGL_MENU_BUILD_APP=OFF` 通常跳过 BSP、硬件、显示、字体、应用视图、Linux
输入后端和 LVGL，但仍构建可主机测试的核心。`LVGL_MENU_BUILD_HEADLESS_TESTS=ON`
只额外加入 LVGL、字体和应用视图，用于可选的首帧无头渲染测试。

## 运行时数据流

```text
GPIO 电气边沿
  -> Linux GPIO v2 事件 fd（内核单调时钟时间戳）
  -> ButtonManager 工作线程：epoll + ButtonStateMachine
  -> 回调：ThreeKeyLvglInput::push()
  -> mutex + deque
  -> 主循环：ThreeKeyLvglInput::pump()
  -> LVGL 所有者线程上的 InputAction 接收器
  -> MenuApplication::handleAction()
  -> 纯 MenuModel 迁移 + 直接更新表现层
  -> LVGL 无效区域
  -> LvglSt7789Display flush 回调
  -> ST7789 地址窗口 + LinuxSpiBus 写入
```

传给 `ButtonManager` 的回调只将字符串 ID 映射为 `PhysicalButton` 并调用
`push()`；它不调用 `lv_*`、不等待动画、不格式化页面，也不执行 SPI I/O。

构造 LVGL 前，`main` 根据在线 CPU 数量和 `/proc/meminfo` 计算与芯片无关的
渲染策略，再应用 CLI 显式覆盖。主循环负责排空输入、更新按键快照、推进应用
时间、调用 LVGL 定时器处理器，以及检查 GPIO 工作线程和显示 flush 错误。
遇到致命输入或 SPI 错误时，资源按所有权逆序释放，并返回非零状态。

第一次 SIGINT/SIGTERM 请求正常退出并设置 SIGALRM 截止时间。信号处理器不使用
`SA_RESTART`，因此被中断的 SPI ioctl 能返回主循环。第二次终止信号或超时才使用
`_exit()`，作为 LVGL 或驱动永不返回时的最后手段。

## Linux GPIO 事件后端

三个按键使用 Linux GPIO v2 上升/下降沿事件。低有效输入申请内部上拉，高有效
覆盖申请内部下拉。Luckfox Linux 5.10 Rockchip GPIO 驱动接受 bias 标志，但不
一定转发给 pinctrl；检测到 RV1103/RV1106 设备树时，后端会通过 `/dev/mem`
写入对应 IOC 两位上下拉字段并回读验证。兼容串、GPIO bank 名、line 范围、
寄存器表和回读必须全部匹配，其他平台不会进入直接寄存器路径。

所有 line fd、一个 timerfd 和停止 eventfd 注册在同一 epoll 中。手势时长使用
内核事件时间戳；`CLOCK_MONOTONIC` 绝对 timerfd 截止时间可在没有新边沿时完成
去抖和长按判断。LCD D/C、复位和背光仍使用 ST7789 参考工程的字符设备输出适配器。

状态机产生 `Press`、`Release`、`Click`、`DoubleClick` 和 `LongPress`。恰好在
长按阈值释放按长按处理，不再激活控件。应用启动前已经按下的键只建立初始稳定
电平，不合成事件；第一次释放的已按时长记为零。

后端校验唯一 ID、唯一 `(chip_path, line_offset)`、line 范围和生命周期。启动
错误同步抛出；后续工作线程错误通过 `lastError()` 暴露给主循环。

## 主线程输入桥

`ThreeKeyLvglInput` 将物理键映射为语义动作：

```cpp
enum class PhysicalButton { Up, Down, Confirm };
enum class InputAction {
    Previous, Next, ConfirmPressed, ConfirmReleased, Activate, Back,
};
```

`push()` 只在互斥锁下追加原始事件。`pump()` 将共享队列交换到主线程本地队列，
保持 FIFO，更新 `ButtonSnapshot`，并在主线程调用动作/遥测接收器。导航按下立即
生效；连发在 `repeat_delay` 后以 `repeat_period` 运行，主循环延迟时最多补发
一次。Up 和 Down 同时按下暂停连发，释放一个方向后按一个周期恢复。

| 原始确认键事件 | 语义动作 |
|---|---|
| `Press` | `ConfirmPressed`，立即显示压缩反馈 |
| `Release` | `ConfirmReleased`，执行释放/回弹动画 |
| `Click` | `Activate`，除非本次按住已产生长按 |
| `LongPress` | 每次按住只产生一次 `Back` |
| `DoubleClick` | 不作为独立命令 |

## 菜单模型

`MenuModel` 是纯状态机。`MenuSnapshot` 是测试和 LVGL 视图使用的完整可观测状态：
当前页面、浏览/编辑模式、主页选择、详情焦点、圆弧数值、两个开关、动画速度、
播放状态、单调 revision 和主页边界反馈计数。

- **主页**：Previous/Next 在四项间循环；Activate 进入详情；Back 只增加边界反馈。
- **圆弧**：Activate 切换浏览/编辑；编辑时每次增减 5 并钳位到 0-100；Back
  先退出编辑，再返回主页。
- **开关**：焦点在两个开关间循环，Activate 切换。第一个控制呼吸动画，第二个
  控制焦点高亮。
- **动画**：焦点在速度和播放/暂停间循环；速度编辑时在慢/中/快三档循环；
  Back 先退出编辑，再回主页。
- **按键**：只读显示三个 `ButtonSnapshot`，Back 返回主页。

## LVGL 视图与动画

`MenuApplication::create()` 只在 LVGL 所有者线程构建一次视图。
`handleAction()` 驱动模型和即时按压反馈；`tick()` 推进小型本地动画，不创建
第二个 UI 线程。

| 动画 | 时长 | 行为 |
|---|---:|---|
| 主页焦点框 | 160 ms | ease-out 到最新静态行 |
| 确认按下 | 70 ms | Low/Balanced 为 2 px/边框脉冲，Quality 为 96% 缩放 |
| 确认释放 | 130 ms | overshoot 回到原尺寸 |
| 页面前进/返回 | 220 ms | 定向滑动 |
| 圆弧/数值变化 | 140 ms | 可重定向插值 |
| 呼吸动画 | 900 ms | 有界循环 |

`retargetAnimation` 会先删除同一对象/属性的旧动画，再启动替换动画；不得为每个
连发事件排队动画。纯 `MenuModel` 是唯一导航状态源。主页行保持静态，只移动独立
焦点框，从而限制脏区。避免大阴影、全屏半透明层和永久全屏失效。

显示桥使用 RGB565 和按 profile 决定的局部缓冲/刷新周期。它始终完成 LVGL
flush 握手，但会粘住第一次 SPI 错误，主循环据此干净地非零退出。

## CLI 校验与资源生命周期

打开 Linux 资源前完成参数解析。正常运行要求 LCD D/C 和三个按键 line；
`--benchmark-home` 只要求显示路径，确定性切换主页焦点并报告 SPI 指标。时长和
尺寸必须符合底层接口，旋转只允许 0/90/180/270；同一 gpiochip 中一个物理 line
不得承担两个角色。

构造顺序为 SPI/显示 GPIO、ST7789/显示桥、LVGL 应用、输入路由、`ButtonManager`。
销毁时先停止并等待 GPIO 工作线程，再销毁队列接收器和 LVGL 对象。

## 自适应渲染与内存

`RenderProfile` 为 Auto、Low、Balanced 或 Quality，不绑定具体芯片。资源字段
缺失时 Auto 安全回退 Low；单核/128 MiB 总量/64 MiB 可用边界选择 Low，
双核/256 MiB/128 MiB 选择 Balanced，更高资源选择 Quality，任一低资源条件优先。

| Profile | 缓冲 | 目标 | 附加 TLSF 池 | 图层行为 |
|---|---:|---:|---:|---|
| Low | 40 行 | 30 FPS | 32 KiB | 禁用整控件图层，只移动圆点 |
| Balanced | 48 行 | 40 FPS | 64 KiB | 禁用整控件图层，允许小对象脉冲 |
| Quality | 60 行 | 50 FPS | 128 KiB | 预算检查通过后允许大效果 |

CLI 缓冲/FPS/heap 覆盖 profile。CMake 从 `lv_conf.h.in` 生成 `lv_conf.h`；
`LVGL_MENU_BASE_HEAP_KIB` 控制内建 TLSF 池，默认 64 KiB。`lv_init` 后以 RAII
对齐存储调用 `lv_mem_add_pool`，其生命周期长于 `lv_deinit`。

首帧同步渲染并记录耗时、LVGL 总量、峰值、空闲、最大连续块和碎片率。可选统计
还记录 flush 字节/时间和 timer handler 时间。启用压缩字体和 LVGL 警告；
断言记录后 abort，不进入默认无限循环。

## 添加页面或控件

1. 在 `MenuPage`/`MenuSnapshot` 增加页面身份和内存内可序列化字段。
2. 在纯 `MenuModel` 定义浏览、编辑、激活和返回行为，不引入 LVGL 类型。
3. 为进入、焦点循环/边界、编辑、激活和两阶段返回增加主机测试。
4. 在 `MenuApplication` 增加中文主页标签和页面构建器，按导航顺序放置控件。
5. 复用可重定向动画助手，不增加页面私有时序常量。
6. 新增字符必须加入字体子集命令，并在两个字号实际使用时重新生成。
7. 重建主机测试和 ARM Release，再在板端测试快速/重复输入。

绝不能从 `ButtonManager` 回调调用 LVGL。新手势先扩展纯按键状态机及测试，再在
`ThreeKeyLvglInput` 中映射。

## 字体资源

仓库中的 `lv_font_ui_cn_16` 和 `lv_font_ui_cn_20` 是生成的 LVGL 位图子集，
源字体为 `C:/Windows/Fonts/NotoSansSC-VF.ttf`。Noto Sans SC 使用
SIL Open Font License 1.1；分发或重生成衍生字体时须保留
[`FONT_LICENSE_cn.md`](../src/assets/fonts/FONT_LICENSE_cn.md) 中的声明和署名。

使用临时 `lv_font_conv` 工具环境生成字体，不得将 Node.js、TTF 或网络依赖加入
应用构建。字符列表只包含 UI 实际显示文本和英文按键事件名。文本变化后应更新
`$symbols`、重新生成两个字号、审查生成文件头命令，并在目标板验证所有页面；
编译成功无法发现缺字。完整生成命令见英文版本及生成文件头。

## 验证矩阵

| 层级 | 必需证据 |
|---|---|
| 纯按键测试 | 去抖、抖动取消、长按阈值、单/双击顺序、极性 |
| 输入桥测试 | FIFO、生产者隔离、快照、连发、方向冲突、确认键互斥 |
| 菜单测试 | 焦点循环、页面进入/返回、编辑保留、0/100 钳位、开关、动画控制 |
| 渲染策略测试 | Auto 阈值、数据缺失回退、覆盖、默认值、图层预算 |
| 无头 LVGL | Low/40 行首帧、受限主页脏区、快速重定向收敛、页面动画结束、heap 可用 |
| ARM 产物 | ELF32 ARM hard-float、Release、静态、无 `INTERP` |
| 板端 | GPIO 极性/offset、显示稳定、快速输入响应、无动画积压、致命 I/O 退出 |

软件输入目标是在配置去抖结束后 50 ms 内出现可见响应。板端目标为两秒内完成
首帧、受限 profile 空闲 CPU 低于 30%、十分钟无 heap 耗尽，并在 40 MHz SPI
下达到 profile 的 30/40/50 FPS。以上是验收目标，不是硬实时保证。
