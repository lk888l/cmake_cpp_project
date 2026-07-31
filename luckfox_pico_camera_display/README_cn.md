# Luckfox Pico RV1103 CSI 摄像头显示

**中文** | [English](README_en.md)

面向 Luckfox Pico RV1103 的生产型 C++17 图像管线：

```text
SC3336 -> RKAIQ/ISP -> Rockit VI（NV12 DMABUF）
       -> RGA（RGB565、裁剪/缩放）-> 最新帧邮箱
       -> Linux spidev -> ST7789 240x240
```

默认将 640x360 完整画面缩放为 240x135，并显示在 `x=0, y=52`。普通视频帧
只发送这一窗口。顶部黑边中的 240x14 状态条每秒刷新一次。稳态视频路径不使用
LVGL、OpenCV、FFmpeg、GStreamer、堆分配或帧队列。

## 运行行为

- 采集/转换、SPI 显示和监管分别运行在独立执行上下文中。
- 两个 RGA 输出是固定的 CMA DMA 缓冲区。邮箱最多暴露一帧就绪帧；显示落后时
  直接覆盖旧帧。
- 正常目标为 30 FPS。持续丢帧或某个统计周期低于 25 FPS 时，进入低延迟
  25 FPS 安全模式；OSD 以黄色文字和 `A` 标记提示。
- 连续三次 VI 超时或 RGA 失败会触发有界媒体重启，退避时间依次为
  250 ms、1 s、2 s；第四次失败退出。
- 第一次 SPI 错误会重置并重新初始化面板；再次失败则以退出码 12 结束。
- `SIGINT` 和 `SIGTERM` 会停止采集、等待两个工作线程、归还 VI 缓冲区、
  销毁 RGA/VI/RKAIQ，并关闭背光。

## 命令

```sh
luckfox_pico_camera_display --probe
luckfox_pico_camera_display --config camera-display.ini --self-test lcd
luckfox_pico_camera_display --config camera-display.ini
luckfox_pico_camera_display --config camera-display.ini --fps 25 --no-osd
```

`--probe` 为只读操作，不占用摄像头、SPI 或 GPIO。LCD 自检显示八条色带和四角
方向色块，并输出实测 SPI 吞吐率。

稳定退出码：

| 退出码 | 含义 |
|---:|---|
| 0 | 正常退出 |
| 2 | CLI 或配置非法 |
| 3 | 预检失败 |
| 4 | 摄像头媒体节点已被其他进程持有 |
| 10 | RKAIQ/VI 摄像头失败 |
| 11 | RGA 失败 |
| 12 | 面板重置一次后 SPI/面板仍失败 |
| 13 | GPIO 申请失败 |
| 14 | 其他运行时失败 |
| 20 | 探测发现必要能力缺失 |

## 构建、部署与操作

固定 SDK、容器和交叉构建流程见
[`docs/luckfox-sdk-build_cn.md`](docs/luckfox-sdk-build_cn.md)。

最近一次可复现构建和板端证据见
[`docs/validation-report_cn.md`](docs/validation-report_cn.md)。

仅主机逻辑测试：

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

将生成的归档部署到 `/userdata/camera-display` 后使用：

```sh
/userdata/camera-display/camera-displayctl probe
/userdata/camera-display/camera-displayctl self-test
/userdata/camera-display/camera-displayctl start
/userdata/camera-display/camera-displayctl status
/userdata/camera-display/camera-displayctl stop
```

控制器会记录 `rkipc` 是否正在运行，向它发送 `TERM`，等待摄像头释放后启动本
程序，并在停止时恢复 `rkipc`。应用自身从不终止其他进程，控制脚本也不会更改
开机配置。

应用遥测写入 `camera-display.log`；控制器保留一个最大 1 MiB 的上一代日志
`camera-display.log.1`。恢复后的 `rkipc` 输出不会混入应用日志。

## 源码结构

| 目录 | 职责 |
|---|---|
| `src/core` | 配置、布局、邮箱、指标、恢复、OSD、稳定接口 |
| `src/platform/linux` | 可处理 EINTR 的 spidev 和 GPIO 字符设备适配器 |
| `src/platform/rockchip` | RKAIQ/VI、CMA DMA、RGA、只读探测 |
| `src/drivers` | ST7789 控制器、字节序、局部窗口传输 |
| `src/app` | 生命周期监管、工作线程、CLI 模式 |
| `tests` | 主机假实现和确定性 CTest |
| `deploy` | 兼容 BusyBox 的板端控制器 |

## 范围

本版本运行时独占摄像头。RTSP/VENC、录像、网络推流、NPU 推理、三键输入和永久
修改开机配置均不在本版本范围内。
