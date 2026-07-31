# Reproducible Luckfox SDK and application build

[中文](luckfox-sdk-build_cn.md) | **English**

## Frozen inputs

| Input | Pinned value |
|---|---|
| Luckfox SDK | `LuckfoxTECH/luckfox-pico` |
| SDK commit | `824b817f889c2cbff1d48fcdb18ab494a68f69d1` |
| Board | Luckfox Pico, RV1103 / `RKIPC_RV1103` |
| Toolchain | `arm-rockchip830-linux-uclibcgnueabihf` from the SDK |
| Container | `ubuntu:22.04` |
| Language/build | C++17, CMake >= 3.16, Ninja |

Keep the SDK on WSL2's ext4 filesystem. Do not clone or build it below
`/mnt/c`; that filesystem is much slower for the SDK's large number of files.

```sh
wsl -d Ubuntu-24.04
mkdir -p "$HOME/src"
git clone https://github.com/LuckfoxTECH/luckfox-pico.git \
  "$HOME/src/luckfox-pico"
git -C "$HOME/src/luckfox-pico" checkout --detach \
  824b817f889c2cbff1d48fcdb18ab494a68f69d1
git -C "$HOME/src/luckfox-pico" rev-parse HEAD
```

The SDK and every SDK output directory remain untracked. Only the application,
build recipes, configuration, controller, and notices belong in this
repository.

## Prepare official media output

The board firmware already contains matching shared libraries. The SDK media
output is still required at build time for the corresponding headers and
unversioned linker inputs.

The repository script selects the exact standard Pico SD-card/RV1103 IPC
configuration non-interactively and builds media:

```sh
export LUCKFOX_SDK_ROOT="$HOME/src/luckfox-pico"
./scripts/build-sdk-media.sh
```

It points the SDK's generated `.BoardConfig.mk` at
`project/cfg/BoardConfig_IPC/BoardConfig-SD_CARD-Buildroot-RV1103_Luckfox_Pico-IPC.mk`
and then runs the official `./build.sh media` target.

The expected result is `output/out/media_out` with Rockit, RKAIQ, and RGA
headers/libraries. If a clean SDK has not unpacked the prebuilt toolchain,
run the SDK's normal first-build preparation before `media`; do not substitute
a glibc ARM compiler.

Confirm the toolchain and media inputs:

```sh
find tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf \
  -name arm-rockchip830-linux-uclibcgnueabihf-g++
find output/out/media_out \
  \( -name rk_mpi_vi.h -o -name rk_aiq_user_api2_sysctl.h \
     -o -name im2d.h -o -name 'librockit.so*' \
     -o -name 'librkaiq.so*' -o -name 'librga.so*' \)
```

## Ubuntu 22.04 container

Enable Docker Desktop's WSL integration for `Ubuntu-24.04`, or install a
compatible Docker engine in that distribution. Then:

```sh
cd /mnt/c/kk_data/code/cpp_project/cmake_cpp_project/luckfox_pico_camera_display
export LUCKFOX_SDK_ROOT="$HOME/src/luckfox-pico"
./scripts/container-build.sh
```

The source tree is mounted at `/work/source`, with generated files confined to
its ignored `build/` directory. The SDK is mounted at `/opt/luckfox-sdk`; only
its documented board-selection link and `output/` products are modified. The
single command builds media, cross-compiles the application, runs the ABI gate,
and creates the deployment archive. For a direct WSL build using the same
official compiler:

```sh
export LUCKFOX_SDK_ROOT="$HOME/src/luckfox-pico"
export LUCKFOX_MEDIA_ROOT="$LUCKFOX_SDK_ROOT/output/out/media_out"
./scripts/build-app.sh
```

## ABI and dependency gate

Run the repository verification script on the resulting executable:

```sh
./scripts/verify-binary.sh \
  build/rv1103-release/src/luckfox_pico_camera_display
```

The gate requires:

- ELF32 little-endian ARM EABI5;
- hard-float ABI;
- interpreter `/lib/ld-uClibc.so.0`;
- `librockit.so`, `librkaiq.so`, and `librga.so` as dynamic dependencies;
- no reference to an x86-64 interpreter or host build paths.

The deployment bundle intentionally does not contain the vendor shared
libraries. The runtime resolves the board-tested copies from `/oem/usr/lib`,
which is also the executable's install RPATH.

## Host tests and sanitizers

Windows/MinGW:

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests --output-on-failure
```

Linux sanitizer gate:

```sh
cmake -S . -B build/host-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCAMERA_DISPLAY_BUILD_APP=OFF \
  -DCAMERA_DISPLAY_BUILD_TESTS=ON \
  -DCAMERA_DISPLAY_ENABLE_SANITIZERS=ON
cmake --build build/host-sanitized
ASAN_OPTIONS=detect_leaks=1 \
  ctest --test-dir build/host-sanitized --output-on-failure
```

## Package and deploy

```sh
BUILD_DIR=build/rv1103-release ./scripts/package.sh
scp build/camera-display-rv1103.tar.gz root@BOARD:/userdata/
ssh root@BOARD '
  cd /userdata &&
  gzip -dc camera-display-rv1103.tar.gz | tar -xf - &&
  /userdata/camera-display/camera-displayctl probe
'
```

The archive contains only:

```text
camera-display/
  bin/luckfox_pico_camera_display
  camera-display.ini
  camera-displayctl
  licenses/THIRD_PARTY_NOTICES_cn.md
  licenses/THIRD_PARTY_NOTICES_en.md
```

## Board acceptance sequence

1. Run `camera-displayctl probe` while `rkipc` is still running.
2. Run `camera-displayctl self-test`; visually verify color, corner orientation,
   byte order, and the reported SPI throughput.
3. Run `camera-displayctl start`, inspect `camera-display.log`, then stop it and
   verify that `rkipc` returns.
4. In normal light, run 30 minutes. Require average display FPS >= 29, software
   latency P95 <= 25 ms, drop rate < 1%, and mailbox depth <= 1.
5. Measure glass-to-glass P95 with a high-speed camera; require <= 100 ms. The
   OSD latency is only VI-acquire-to-SPI-complete software latency.
6. Run `scripts/board-soak.sh 28800`. Require RSS <= 10240 KiB without a rising
   trend, OOM, deadlock, or leaked buffers.
7. Inject camera occupation, VI timeout, invalid GPIO/SPI paths, process
   signals, and forced RGA errors. Verify exit codes, three media recoveries,
   one panel reset, bounded cleanup, and `rkipc` restoration.

Adaptive 25 FPS is an explicit yellow/`A` degraded state. It proves safe
degradation but does not count as passing the nominal 30 FPS criterion.

## Common failures

| Symptom | Check |
|---|---|
| exit 4 / camera occupied | stop through `camera-displayctl`; the app never kills the owner |
| `rkisp_mainpath` absent | camera overlay/device tree, sensor ribbon, kernel log |
| RGA import fails | `/dev/rk_dma_heap/rk-dma-heap-cma`, CMA exhaustion, matching `librga` |
| colors swapped | retain validated `bgr=true`; run LCD color bars before camera |
| torn/old output | DMA-BUF CPU sync and two-slot ownership; never add a frame queue |
| SPI write `EINVAL` | keep chunks <= `/sys/module/spidev/parameters/bufsiz` (4096 here) |
| loader cannot find media libs | export `LD_LIBRARY_PATH=/oem/usr/lib` or use controller |
| wrong ELF interpreter | rebuild only with the SDK uClibc toolchain |
