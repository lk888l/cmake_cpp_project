# 可复现的 Luckfox SDK 与应用构建

**中文** | [English](luckfox-sdk-build_en.md)

## 固定输入

| 输入 | 固定值 |
|---|---|
| Luckfox SDK | `LuckfoxTECH/luckfox-pico` |
| SDK 提交 | `824b817f889c2cbff1d48fcdb18ab494a68f69d1` |
| 板型 | Luckfox Pico，RV1103 / `RKIPC_RV1103` |
| 工具链 | SDK 内的 `arm-rockchip830-linux-uclibcgnueabihf` |
| 容器 | `ubuntu:22.04` |
| 语言/构建 | C++17、CMake >= 3.16、Ninja |

请将 SDK 放在 WSL2 的 ext4 文件系统中。不要克隆或构建到 `/mnt/c` 下；SDK
包含大量小文件，该文件系统上的构建速度会明显变慢。

```sh
wsl -d Ubuntu-24.04
mkdir -p "$HOME/src"
git clone https://github.com/LuckfoxTECH/luckfox-pico.git \
  "$HOME/src/luckfox-pico"
git -C "$HOME/src/luckfox-pico" checkout --detach \
  824b817f889c2cbff1d48fcdb18ab494a68f69d1
git -C "$HOME/src/luckfox-pico" rev-parse HEAD
```

SDK 及所有 SDK 输出目录都保持为未跟踪状态。仓库只保存应用、构建配方、配置、
控制器和声明文件。

## 准备官方媒体输出

板载固件已经包含匹配的共享库。构建阶段仍然需要 SDK 的媒体输出，以取得对应
头文件和无版本后缀的链接输入。

仓库脚本会以非交互方式选择准确的标准 Pico SD 卡/RV1103 IPC 配置并构建媒体：

```sh
export LUCKFOX_SDK_ROOT="$HOME/src/luckfox-pico"
./scripts/build-sdk-media.sh
```

脚本将 SDK 生成的 `.BoardConfig.mk` 指向
`project/cfg/BoardConfig_IPC/BoardConfig-SD_CARD-Buildroot-RV1103_Luckfox_Pico-IPC.mk`，
随后运行官方 `./build.sh media` 目标。

预期结果是包含 Rockit、RKAIQ 和 RGA 头文件/库的
`output/out/media_out`。如果干净 SDK 尚未解包预编译工具链，请先执行 SDK
常规首次构建准备；不得用 glibc ARM 编译器替代。

确认工具链和媒体输入：

```sh
find tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf \
  -name arm-rockchip830-linux-uclibcgnueabihf-g++
find output/out/media_out \
  \( -name rk_mpi_vi.h -o -name rk_aiq_user_api2_sysctl.h \
     -o -name im2d.h -o -name 'librockit.so*' \
     -o -name 'librkaiq.so*' -o -name 'librga.so*' \)
```

## Ubuntu 22.04 容器

启用 Docker Desktop 对 `Ubuntu-24.04` 的 WSL 集成，或在该发行版中安装兼容
Docker Engine，然后执行：

```sh
cd /mnt/c/kk_data/code/cpp_project/cmake_cpp_project/luckfox_pico_camera_display
export LUCKFOX_SDK_ROOT="$HOME/src/luckfox-pico"
./scripts/container-build.sh
```

源码树挂载到 `/work/source`，生成文件仅写入其中已被忽略的 `build/` 目录。
SDK 挂载到 `/opt/luckfox-sdk`；只修改其文档化的板型选择链接和 `output/`
产物。单条命令会构建媒体、交叉编译应用、运行 ABI 门禁并生成部署归档。

如需使用同一官方编译器直接在 WSL 中构建：

```sh
export LUCKFOX_SDK_ROOT="$HOME/src/luckfox-pico"
export LUCKFOX_MEDIA_ROOT="$LUCKFOX_SDK_ROOT/output/out/media_out"
./scripts/build-app.sh
```

## ABI 与依赖门禁

对生成的可执行文件运行仓库验证脚本：

```sh
./scripts/verify-binary.sh \
  build/rv1103-release/src/luckfox_pico_camera_display
```

门禁要求：

- ELF32 小端 ARM EABI5；
- 硬浮点 ABI；
- 解释器为 `/lib/ld-uClibc.so.0`；
- 动态依赖包含 `librockit.so`、`librkaiq.so` 和 `librga.so`；
- 不得引用 x86-64 解释器或宿主构建路径。

部署包有意不包含厂商共享库。运行时从 `/oem/usr/lib` 解析板上已验证版本，该
路径也是可执行文件的安装 RPATH。

## 主机测试与 Sanitizer

Windows/MinGW：

```sh
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests --output-on-failure
```

Linux Sanitizer 门禁：

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

## 打包与部署

```sh
BUILD_DIR=build/rv1103-release ./scripts/package.sh
scp build/camera-display-rv1103.tar.gz root@BOARD:/userdata/
ssh root@BOARD '
  cd /userdata &&
  gzip -dc camera-display-rv1103.tar.gz | tar -xf - &&
  /userdata/camera-display/camera-displayctl probe
'
```

归档只包含：

```text
camera-display/
  bin/luckfox_pico_camera_display
  camera-display.ini
  camera-displayctl
  licenses/THIRD_PARTY_NOTICES_cn.md
  licenses/THIRD_PARTY_NOTICES_en.md
```

## 板端验收顺序

1. 保持 `rkipc` 运行，执行 `camera-displayctl probe`。
2. 执行 `camera-displayctl self-test`，目视确认颜色、四角方向、字节序和报告的
   SPI 吞吐率。
3. 执行 `camera-displayctl start`，检查 `camera-display.log`，随后停止并确认
   `rkipc` 恢复。
4. 正常光照下运行 30 分钟。要求平均显示帧率 >= 29、软件延迟 P95 <= 25 ms、
   丢帧率 < 1%，邮箱深度 <= 1。
5. 使用高速摄像机测量玻璃到玻璃 P95，要求 <= 100 ms。OSD 延迟只表示
   VI 取帧到 SPI 完成的软件延迟。
6. 运行 `scripts/board-soak.sh 28800`。要求 RSS <= 10240 KiB 且无上升趋势，
   无 OOM、死锁或缓冲泄漏。
7. 注入摄像头占用、VI 超时、无效 GPIO/SPI 路径、进程信号和强制 RGA 错误。
   验证退出码、三次媒体恢复、一次面板重置、有界清理和 `rkipc` 恢复。

自适应 25 FPS 是明确的黄色/`A` 降级状态。它证明安全降级有效，但不计为标称
30 FPS 性能通过。

## 常见故障

| 现象 | 检查项 |
|---|---|
| 退出码 4 / 摄像头被占用 | 通过 `camera-displayctl` 停止占用；应用不会终止持有者 |
| 缺少 `rkisp_mainpath` | 摄像头 overlay/设备树、传感器排线、内核日志 |
| RGA 导入失败 | `/dev/rk_dma_heap/rk-dma-heap-cma`、CMA 耗尽、匹配的 `librga` |
| 颜色互换 | 保留已验证的 `bgr=true`；摄像头前先运行 LCD 色条 |
| 撕裂/旧帧 | DMA-BUF CPU 同步和双槽所有权；不要增加帧队列 |
| SPI 写入 `EINVAL` | 数据块不得超过 `/sys/module/spidev/parameters/bufsiz`，本板为 4096 |
| 加载器找不到媒体库 | 导出 `LD_LIBRARY_PATH=/oem/usr/lib` 或使用控制器 |
| ELF 解释器错误 | 只能使用 SDK uClibc 工具链重建 |
