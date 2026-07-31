# Validation report

[中文](validation-report_cn.md) | **English**

Validation date: 2026-07-31

## Reproducible build

- Luckfox SDK commit:
  `824b817f889c2cbff1d48fcdb18ab494a68f69d1`
- Build environment: pinned `ubuntu:22.04` container
- Compiler: SDK `arm-rockchip830-linux-uclibcgnueabihf` GCC 8.3
- ELF gate: ELF32 little-endian ARM EABI5, hard-float, interpreter
  `/lib/ld-uClibc.so.0`
- Runtime dependencies: `librockit.so`, `librkaiq.so`, `librga.so`,
  `libstdc++.so.6`, `libgcc_s.so.1`, and `libc.so.0`
- Runtime RPATH: `/oem/usr/lib`; no SDK, container, Windows, or WSL path

Windows host CTest and the WSL ASan/UBSan/LeakSanitizer build pass.

## Hardware smoke and pipeline results

Test board: Luckfox Pico RV1103 with the enabled SC3336 CSI pipeline and the
existing 240x240 ST7789 wiring.

| Check | Observed result |
|---|---|
| Read-only probe | `/dev/video11` exact `rkisp_mainpath`; RGA 1.10.1 / RGA_2_Enhance; NV12 input and RGB565 output supported |
| LCD self-test | 40367 us full-frame transfer, 2.72161 MiB/s, zero SPI errors |
| Full pipeline | 640x360 NV12 DMABUF -> RGA 240x135 RGB565 -> partial-window SPI |
| 60-second smoke soak | 58 complete metric intervals, average 29.998 FPS, zero dropped frames |
| Software latency | steady per-second P95 22-24 ms; one 27 ms startup/transient interval |
| Memory | RSS 7160-7164 KiB across six 10-second samples |
| Threads | 14 total: three application threads plus Rockit/RKAIQ worker threads |
| Shutdown | direct SIGTERM completed in 1 second; PID file removed |
| Service restoration | `rkipc` restored after every controller stop/failure test |
| Invalid SPI path | failed before hardware access with exit code 3; `rkipc` unchanged |
| Occupied camera | detected `/dev/video11` owner through `/proc/*/fd`, returned exit code 4 immediately; owner unchanged |

Operational health records are emitted with one allocation-free `write` per
line, so concurrent vendor logging cannot split a metric record.

## Acceptance still requiring controlled hardware time

The 30-minute nominal-light run, 8-hour soak, forced RGA/VI fault injection,
and high-speed-camera glass-to-glass measurement have not been claimed here.
Use the documented acceptance sequence and `scripts/board-soak.sh` for those
release gates. The physical LCD corner colors and camera image orientation
must also be visually confirmed by an operator at the board.
