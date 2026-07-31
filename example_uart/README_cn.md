# example_uart

**中文** | [English](README_en.md)

`example_uart` 是面向 Luckfox/Linux ARM32 的现代 C++23 UART 示例。工程使用 BSP 与业务代码分层：业务层只依赖可移植 UART 接口，Linux 的设备扫描、`termios`、文件描述符和 `epoll` 都封装在 BSP 实现中。

## 分层结构

```text
example_uart/
├── cmake/
│   └── arm-none-linux-gnueabihf.cmake
├── src/
│   ├── app/
│   │   ├── uart_demo_app.hpp
│   │   └── uart_demo_app.cpp
│   ├── bsp/uart/
│   │   ├── uart_port.hpp
│   │   ├── linux_uart_port.hpp
│   │   └── linux_uart_port.cpp
│   ├── tests/
│   │   └── uart_demo_app_tests.cpp
│   ├── CMakeLists.txt
│   └── main.cpp
├── .clang-format
└── CMakeLists.txt
```

依赖方向如下：

```text
main（设备选择和对象装配）
  ├── app::UartDemoApp ──> bsp::UartPort（抽象接口）
  └── bsp::LinuxUartPort ──> Linux termios + epoll
```

- `bsp::UartPort`：定义业务层能够使用的 `write()` 与 `waitAndRead()` 接口。
- `bsp::LinuxUartPort`：RAII 管理 UART 和 epoll 文件描述符，配置 8N1、无流控、非阻塞 UART。
- `bsp::discoverSerialPorts()`：扫描并排序 `/dev/ttyS*`、`ttyUSB*`、`ttyACM*`、`ttyAMA*` 和 `ttyO*`。
- `app::UartDemoApp`：发送欢迎字符串，阻塞等待一批数据，并将非空数据交给接收回调。
- `main.cpp`：显示串口列表、校验用户选择、创建 Linux BSP 并使用 `std::println` 输出数据。

业务层不包含 Linux 头文件，也不访问文件描述符，因此可以使用 Fake UART 在开发机上测试。

## 运行流程

1. 程序扫描 `/dev` 下支持的串口设备并按路径排序。
2. 用户输入列表中的串口编号；非数字和越界输入会重新提示。
3. `LinuxUartPort` 打开设备并配置为 115200、8 数据位、无校验、1 停止位、无流控。
4. UART 文件描述符注册到 epoll，业务层发送 `Hello from example_uart(epoll)!`。
5. 程序永久阻塞在 epoll；收到数据后输出 `Received: ...`，等待期间不会忙轮询。

如果没有找到串口或标准输入结束，程序正常退出；打开、配置、读写或 epoll 失败时打印错误并返回非零状态。

## ARM 交叉构建

工程默认使用 `arm-none-linux-gnueabihf` 和 Release 静态链接。Windows PowerShell 中执行：

```powershell
cmake -S . -B build-arm -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf
cmake --build build-arm -j
```

如果工具链不在 `PATH` 或默认搜索目录中，通过 `TOOLCHAIN_PATH` 指定其 `bin` 目录：

```powershell
cmake -S . -B build-arm -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf `
  -DTOOLCHAIN_PATH=C:/path/to/arm-none-linux-gnueabihf/bin
```

生成的程序位于 `build-arm/src/example_uart`。可使用工具链的 `readelf` 验证架构和静态链接：

```powershell
arm-none-linux-gnueabihf-readelf -h build-arm/src/example_uart
arm-none-linux-gnueabihf-readelf -l build-arm/src/example_uart
```

ELF 头应显示 ARM 32 位；静态版本的程序头中不应出现 `INTERP`。

## 本机业务测试

在 Windows 上，`local` 配置只构建不依赖 POSIX 的业务层和 Fake UART 测试：

```powershell
cmake -S . -B build-local -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=local `
  -DBUILD_TESTING=ON
cmake --build build-local -j
ctest --test-dir build-local --output-on-failure
```

在本机 Linux 上，`local` 配置还会构建可直接运行的 `example_uart`。

## 代码风格

工程保持 C++23，并使用 `std::print/std::println` 的 `{}` 占位符格式化输出，不依赖 `{fmt}`。`.clang-format` 规定 4 空格缩进、100 列、函数 Allman 大括号和自动排序头文件。命名约定如下：

- 类型：`UpperCamelCase`
- 函数：`lowerCamelCase`
- 局部变量和成员变量：`snake_case`
- 常量：`kUpperCamelCase`
