# RollerCAN I2C 校准器

**中文** | [English](README_en.md)

独立的 Linux/Buildroot C++ 工具，通过原固件 I2C 协议重新校准 RollerCAN 编码器
电角度偏移。工程结构参考 `example_icm42688`：Linux 总线访问位于 `Peripheral`，
RollerCAN 协议位于 `HardWare`，校准流程位于 `App`。

## 安全

校准会以约 1.2 A 相电流给电机通电约三秒。移除机械负载、固定设备、手远离轴，
并优先使用限流电源。校准时不要进入前面板 SmartKnob 设置模式；RollerCAN 必须
以 I2C 模式运行正常应用。

## 接线

| Buildroot 板 | RollerCAN Grove I2C |
|---|---|
| GND | GND / 黑 |
| SDA，3.3 V 逻辑 | SDA / 黄 |
| SCL，3.3 V 逻辑 | SCL / 白 |

RollerCAN 通过支持的电源输入单独供电。不要将 I2C 上拉到 5 V。默认 7 位地址为
`0x64`。

## Windows ARM 交叉构建

默认使用 `arm-none-linux-gnueabihf-g++` 和静态链接：

```powershell
cmake -G "MinGW Makefiles" -S . -B build
cmake --build build --parallel
```

编译器不在 `PATH` 或 `C:/kk_software/toolchain` 时：

```powershell
cmake -G "MinGW Makefiles" -S . -B build `
  -DTOOLCHAIN_PATH=C:/path/to/toolchain/bin
cmake --build build --parallel
```

不同 Buildroot 前缀或 sysroot：

```powershell
cmake -G "MinGW Makefiles" -S . -B build `
  -DTOOLCHAIN_PATH=C:/buildroot/host/bin `
  -DTOOLCHAIN_PREFIX=arm-buildroot-linux-gnueabihf `
  -DSYSROOT_PATH=C:/buildroot/host/arm-buildroot-linux-gnueabihf/sysroot
cmake --build build --parallel
```

输出为 `build/src/rollercan_i2c_calibrator`。Release 默认 strip；调试符号需要
`-DSTRIP_RELEASE_BINARY=OFF`。工具链没有静态 C/C++ 运行库时使用
`-DUSE_STATIC_LINKING=OFF` 并部署匹配共享库。

## 部署与运行

```sh
ls -l /dev/i2c-*
chmod +x /tmp/rollercan_i2c_calibrator
/tmp/rollercan_i2c_calibrator --probe --bus /dev/i2c-3 --addr 0x64
```

预期探测输出包含：

```text
I2C communication OK; calibration_busy=0
```

校准会给电机通电，因此必须显式使用 `--yes`：

```sh
/tmp/rollercan_i2c_calibrator --yes --bus /dev/i2c-3 --addr 0x64
```

工具执行原固件的准确序列：

1. `00 00`：关闭电机输出。
2. `F1 01`：开始编码器校准。
3. 发送带 STOP 的 `F3`，再单独读一个字节，直到从 1 变为 0。
4. `F2 01`：应用偏移并保存到 RollerCAN flash。
5. `00 00`：保持输出关闭。

成功后重启 RollerCAN，以低限流和低正/负转速测试，再施加正常负载。

## 故障排查

- `not_found`：I2C 节点/地址错误、未共地或没有 ACK。
- `Permission denied`：使用 root 或调整 i2c-dev 权限。
- `calibration_not_started`：检查电源、I2C 模式、固件及前面板设置菜单。
- `timeout`：立即断电，检查电机自由度、电源和固件后再尝试。
