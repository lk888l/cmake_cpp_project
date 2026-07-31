# example_icm42688

**中文** | [English](README_en.md)

通过 I2C 使用 ICM42688 的 Linux CMake 示例。工程将平台总线代码、传感器驱动、
应用循环和 VQF 融合代码划分为独立层。

## 构建

默认目标使用 `arm-none-linux-gnueabihf-gcc/g++` 和静态链接。目标板根文件系统
使用 uClibc 而非 glibc 时，静态链接可避免依赖板端动态加载器。

```bash
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
```

若工具链不在 `PATH` 中，请传入其 `bin` 目录：

```bash
cmake -G "MinGW Makefiles" -S . -B build -DTOOLCHAIN_PATH=/path/to/toolchain/bin
cmake --build build
```

在 Linux 主机上进行本机语法检查：

```bash
cmake -S . -B build-local -DTARGET_PLATFORM=local
cmake --build build-local
```

## 运行

默认使用 `/dev/i2c-3`、地址 `0x68` 和 10 ms 采样周期。

```bash
./example_icm42688
./example_icm42688 --bus /dev/i2c-3 --addr 0x68 --period-ms 10
```

CSV 输出：

```text
quat_w,quat_x,quat_y,quat_z,roll_deg,pitch_deg,yaw_deg,temp_c,accel_x_g,accel_y_g,accel_z_g,gyro_x_dps,gyro_y_dps,gyro_z_dps
```
