# Luckfox Pico RV1103 CSI camera display

[中文](README_cn.md) | **English**

Production-oriented C++17 pipeline for a Luckfox Pico RV1103:

```text
SC3336 -> RKAIQ/ISP -> Rockit VI (NV12 DMABUF)
       -> RGA (RGB565, crop/scale) -> latest-frame mailbox
       -> Linux spidev -> ST7789 240x240
```

The default 640x360 frame is shown as a complete 240x135 image at
`x=0, y=52`. Only that window is sent for normal video frames. A 240x14
status strip is refreshed once per second in the top black bar. No LVGL,
OpenCV, FFmpeg, GStreamer, heap allocation, or frame queue is used in the
steady-state video path.

## Runtime behavior

- Capture/convert, SPI presentation, and supervision are separate execution
  contexts.
- The two RGA outputs are fixed CMA DMA buffers. The mailbox exposes at most
  one ready frame and overwrites it when the display is behind.
- The normal target is 30 FPS. Sustained drops or a sub-25 FPS interval cause
  a low-latency 25 FPS safety mode; `A` and yellow text appear in the OSD.
- Three consecutive VI timeouts or RGA failures trigger a bounded media
  restart. Backoffs are 250 ms, 1 s, and 2 s. A fourth failure exits.
- The first SPI error resets and reinitializes the panel. Another SPI failure
  exits with code 12.
- `SIGINT` and `SIGTERM` stop acquisition, join both workers, return VI
  buffers, tear down RGA/VI/RKAIQ, and turn off the backlight.

## Commands

```sh
luckfox_pico_camera_display --probe
luckfox_pico_camera_display --config camera-display.ini --self-test lcd
luckfox_pico_camera_display --config camera-display.ini
luckfox_pico_camera_display --config camera-display.ini --fps 25 --no-osd
```

`--probe` is read-only and does not claim the camera, SPI, or GPIO. The LCD
self-test shows eight color bars plus colored corner direction markers and
prints measured SPI throughput.

Exit codes are stable:

| Code | Meaning |
|---:|---|
| 0 | clean exit |
| 2 | CLI or configuration invalid |
| 3 | preflight failed |
| 4 | camera media node is already held by another process |
| 10 | RKAIQ/VI camera failure |
| 11 | RGA failure |
| 12 | SPI/panel failure after one reset |
| 13 | GPIO request failure |
| 14 | other runtime failure |
| 20 | probe found missing capability |

## Build, deploy, and operate

The pinned SDK/container/cross-build procedure is in
[`docs/luckfox-sdk-build_en.md`](docs/luckfox-sdk-build_en.md).

The latest reproducible build and board evidence is recorded in
[`docs/validation-report_en.md`](docs/validation-report_en.md).

Host-only logic:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

Deploy the generated archive under `/userdata/camera-display`, then use:

```sh
/userdata/camera-display/camera-displayctl probe
/userdata/camera-display/camera-displayctl self-test
/userdata/camera-display/camera-displayctl start
/userdata/camera-display/camera-displayctl status
/userdata/camera-display/camera-displayctl stop
```

The controller records whether `rkipc` was running, sends it `TERM`, waits for
camera release, starts this application, and restores `rkipc` on stop. The
application itself never kills another process and the script does not alter
boot configuration.

Application telemetry is written to `camera-display.log`; the controller keeps
one 1 MiB previous generation as `camera-display.log.1`. Restored `rkipc`
output is deliberately detached from these application logs.

## Source layout

| Directory | Responsibility |
|---|---|
| `src/core` | configuration, layout, mailbox, metrics, recovery, OSD, stable interfaces |
| `src/platform/linux` | EINTR-safe spidev and GPIO character-device adapters |
| `src/platform/rockchip` | RKAIQ/VI, CMA DMA, RGA, and read-only probe |
| `src/drivers` | ST7789 controller, byte order, partial-window transfer |
| `src/app` | lifecycle supervision, worker threads, CLI modes |
| `tests` | host fakes and deterministic CTest coverage |
| `deploy` | BusyBox-compatible board controller |

## Scope

This release exclusively owns the camera while running. RTSP/VENC, recording,
network streaming, NPU inference, three-key input, and permanent boot changes
are intentionally outside its boundary.
