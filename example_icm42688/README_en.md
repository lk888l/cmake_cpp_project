# example_icm42688

[中文](README_cn.md) | **English**

Linux CMake demo for ICM42688 over I2C. The project keeps the platform bus code,
sensor driver, application loop, and VQF fusion code in separate layers.

## Build

The default build target uses `arm-none-linux-gnueabihf-gcc/g++` and static
linking. Static linking avoids depending on the target board's dynamic loader,
which is important when the board rootfs uses uClibc instead of glibc.

```bash
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build
```

If the toolchain is not in `PATH`, pass its bin directory:

```bash
cmake -G "MinGW Makefiles" -S . -B build -DTOOLCHAIN_PATH=/path/to/toolchain/bin
cmake --build build
```

For a local syntax check on a Linux host:

```bash
cmake -S . -B build-local -DTARGET_PLATFORM=local
cmake --build build-local
```

## Run

Defaults are `/dev/i2c-3`, address `0x68`, and a 10 ms sample period.

```bash
./example_icm42688
./example_icm42688 --bus /dev/i2c-3 --addr 0x68 --period-ms 10
```

CSV output:

```text
quat_w,quat_x,quat_y,quat_z,roll_deg,pitch_deg,yaw_deg,temp_c,accel_x_g,accel_y_g,accel_z_g,gyro_x_dps,gyro_y_dps,gyro_z_dps
```
