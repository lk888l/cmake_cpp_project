# 交叉编译工具链使用指南

## 概述

该项目支持三种编译方式：

1. **arm-linux**（Buildroot aarch64-buildroot-linux-gnu）
2. **aarch64-gnu**（标准 aarch64-none-linux-gnu 工具链） ← 新增
3. **local-x86**（本地开发编译）

---

## 使用 aarch64-none-linux-gnu 工具链编译

### 前置要求

确保已安装 aarch64-none-linux-gnu 工具链：

```bash
# Ubuntu/Debian
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# 或者手动安装到自定义位置
# 从 https://developer.arm.com/Tools-and-Software/GNU-Toolchain 下载
```

### 方案 A：从 PATH 自动查找工具链（推荐）

如果工具已安装到系统 PATH（如 `/usr/bin`），直接执行：

```bash
cmake -S . -B build \
    -DTARGET_PLATFORM=aarch64-gnu \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 方案 B：指定自定义工具链路径

如果工具链在非标准位置，指定 `TOOLCHAIN_PATH`：

```bash
cmake -S . -B build \
    -DTARGET_PLATFORM=aarch64-gnu \
    -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-none-linux-gnu.cmake \
    -DTOOLCHAIN_PATH=/path/to/toolchain/bin \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 方案 C：使用 sysroot（可选）

如果需要指定目标系统根文件系统（用于库和头文件链接）：

```bash
cmake -S . -B build \
    -DTARGET_PLATFORM=aarch64-gnu \
    -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-none-linux-gnu.cmake \
    -DSYSROOT_PATH=/path/to/sysroot \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## 编译器选择对比

### aarch64-buildroot-linux-gnu（现有 arm-linux）

**特点：**
- 来自 Buildroot 构建系统
- 集成了 Buildroot 生成的 sysroot
- 适合 Buildroot 交叉编译环境

**编译命令：**
```bash
cmake -S . -B build \
    -DTARGET_PLATFORM=arm-linux \
    -DBUILDROOT_DIR=/path/to/buildroot/output
cmake --build build -j$(nproc)
```

### aarch64-none-linux-gnu（新增 aarch64-gnu）✨

**特点：**
- 标准 GNU Arm 工具链（官方维护）
- 不依赖 Buildroot
- 更灵活，可单独指定 sysroot
- 遵循 CMake 官方最佳实践

**编译命令：**
```bash
cmake -S . -B build \
    -DTARGET_PLATFORM=aarch64-gnu \
    [-DSYSROOT_PATH=/path/to/sysroot]
cmake --build build -j$(nproc)
```

---

## 工具链文件设计原则

新的 `aarch64-none-linux-gnu.cmake` 遵循以下原则：

### 1. 现代 CMake 标准（3.20+）

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)  # 交叉编译优化
```

### 2. 灵活的工具链查找

```cmake
# 自动从 PATH 查找
find_program(CMAKE_C_COMPILER NAMES aarch64-none-linux-gnu-gcc REQUIRED)

# 或从指定目录查找
set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/aarch64-none-linux-gnu-gcc")
```

### 3. 标准的搜索路径策略

```cmake
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)   # 工具从主机查找
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)    # 库仅从 sysroot 查找
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)    # 头文件仅从 sysroot 查找
```

### 4. 编译和链接标志

```cmake
# 默认 ARM v8 架构
set(CMAKE_C_FLAGS_INIT "-march=armv8-a")

# 如果指定 sysroot，自动添加相应标志
set(CMAKE_EXE_LINKER_FLAGS_INIT "--sysroot=${CMAKE_SYSROOT}")
```

### 5. pkg-config 支持

```cmake
set(ENV{PKG_CONFIG_SYSROOT_DIR} ${CMAKE_SYSROOT})
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:...")
```

---

## 参考资源

### CMake 官方文档

- [CMake Toolchains Manual](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [CMake Compile Features](https://cmake.org/cmake/help/latest/manual/cmake-compile-features.7.html)
- [Cross Compiling Guide](https://cmake.org/cmake/help/latest/manual/cmake.1.html#cross-compiling)

### GNU Arm Toolchain

- [GNU Arm Embedded Toolchain](https://developer.arm.com/Tools-and-Software/GNU-Toolchain)
- [aarch64 Linux GNU Documentation](https://gcc.gnu.org/)

---

## 常见问题

### Q: 如何验证使用了正确的编译器？

```bash
# 查看 CMake 输出
cmake -S . -B build -DTARGET_PLATFORM=aarch64-gnu -DCMAKE_BUILD_TYPE=Release

# 输出会显示：
# C Compiler:          /usr/bin/aarch64-none-linux-gnu-gcc
# C++ Compiler:        /usr/bin/aarch64-none-linux-gnu-g++
```

### Q: 如何在 CI/CD 中使用？

```yaml
# GitHub Actions 示例
- name: Compile with aarch64-gnu
  run: |
    cmake -S . -B build \
      -DTARGET_PLATFORM=aarch64-gnu \
      -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j4
```

### Q: 静态链接还是动态链接？

```bash
# 两个工具链的默认配置都是静态链接
# 若要禁用，修改 CMakeLists.txt 中的 USE_STATIC_LINKING：
set(USE_STATIC_LINKING OFF CACHE BOOL "Use static linking")
```

### Q: 找不到工具链怎么办？

确保以下之一：

1. 工具已添加到 PATH
2. 通过 `-DTOOLCHAIN_PATH` 明确指定路径
3. 检查工具链是否真的安装：`which aarch64-none-linux-gnu-gcc`

```bash
# 验证工具链安装
aarch64-none-linux-gnu-gcc --version
aarch64-none-linux-gnu-g++ --version
```

---

## 编译示例

### 快速编译

```bash
# 使用默认 sysroot 和 Release 优化
cd /path/to/example_thread
cmake -S . -B build -DTARGET_PLATFORM=aarch64-gnu
cmake --build build -j$(nproc)
ls -lh build/src/example_thread  # 查看生成的可执行文件
```

### 带调试符号编译

```bash
cmake -S . -B build \
    -DTARGET_PLATFORM=aarch64-gnu \
    -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### 部署到目标设备

```bash
# 复制编译结果到设备
scp build/src/example_thread root@172.18.25.50:/home/
```
