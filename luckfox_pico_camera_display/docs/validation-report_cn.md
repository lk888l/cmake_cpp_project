# 验证报告

**中文** | [English](validation-report_en.md)

验证日期：2026-07-31

## 可复现构建

- Luckfox SDK 提交：
  `824b817f889c2cbff1d48fcdb18ab494a68f69d1`
- 构建环境：固定的 `ubuntu:22.04` 容器
- 编译器：SDK 中的 `arm-rockchip830-linux-uclibcgnueabihf` GCC 8.3
- ELF 门禁：ELF32 小端 ARM EABI5、硬浮点，解释器为
  `/lib/ld-uClibc.so.0`
- 运行时依赖：`librockit.so`、`librkaiq.so`、`librga.so`、
  `libstdc++.so.6`、`libgcc_s.so.1` 和 `libc.so.0`
- 运行时 RPATH：`/oem/usr/lib`；不包含 SDK、容器、Windows 或 WSL 路径

Windows 主机 CTest 以及 WSL ASan/UBSan/LeakSanitizer 构建均通过。

## 硬件冒烟和管线结果

测试板：Luckfox Pico RV1103，已启用 SC3336 CSI 管线，沿用现有 240x240
ST7789 接线。

| 检查项 | 实测结果 |
|---|---|
| 只读探测 | `/dev/video11` 精确对应 `rkisp_mainpath`；RGA 1.10.1 / RGA_2_Enhance；支持 NV12 输入和 RGB565 输出 |
| LCD 自检 | 整屏传输 40367 us，2.72161 MiB/s，SPI 错误为零 |
| 完整管线 | 640x360 NV12 DMABUF -> RGA 240x135 RGB565 -> SPI 局部窗口 |
| 60 秒冒烟稳定性 | 58 个完整指标周期，平均 29.998 FPS，丢帧为零 |
| 软件延迟 | 稳态每秒 P95 为 22-24 ms；有一个 27 ms 的启动/瞬态周期 |
| 内存 | 六次 10 秒采样中 RSS 为 7160-7164 KiB |
| 线程 | 共 14 条：三条应用线程，加 Rockit/RKAIQ 工作线程 |
| 退出 | 直接 SIGTERM 在 1 秒内完成；PID 文件已删除 |
| 服务恢复 | 每次控制器停止/故障测试后都恢复 `rkipc` |
| 无效 SPI 路径 | 触碰硬件前以退出码 3 失败；`rkipc` 不变 |
| 摄像头占用 | 通过 `/proc/*/fd` 找到 `/dev/video11` 持有者，立即返回退出码 4；持有者不变 |

运行健康记录每行只执行一次无分配 `write`，因此厂商并发日志不会拆分指标记录。

## 仍需受控硬件时间的验收

本文未声称已经完成正常光照 30 分钟测试、8 小时稳定性测试、强制 RGA/VI 故障
注入和高速摄像机玻璃到玻璃延迟测量。这些发布门禁请按构建文档中的验收流程及
`scripts/board-soak.sh` 执行。LCD 四角实际颜色和摄像头画面方向也必须由板端
操作人员目视确认。
