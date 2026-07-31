# example_uart

[中文](README_cn.md) | **English**

`example_uart` is a modern C++23 UART example for Luckfox/Linux ARM32. The
project separates the BSP from business logic: the application depends only
on a portable UART interface, while Linux device discovery, `termios`, file
descriptors, and `epoll` remain inside the BSP implementation.

## Layered structure

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

Dependency direction:

```text
main (device selection and object assembly)
  ├── app::UartDemoApp ──> bsp::UartPort (abstract interface)
  └── bsp::LinuxUartPort ──> Linux termios + epoll
```

- `bsp::UartPort` defines the `write()` and `waitAndRead()` API available to
  application logic.
- `bsp::LinuxUartPort` uses RAII for UART and epoll descriptors and configures
  non-blocking 8N1 without flow control.
- `bsp::discoverSerialPorts()` scans and sorts `/dev/ttyS*`, `ttyUSB*`,
  `ttyACM*`, `ttyAMA*`, and `ttyO*`.
- `app::UartDemoApp` sends a greeting, waits for a batch of data, and sends
  non-empty input to the receive callback.
- `main.cpp` lists ports, validates the selection, creates the Linux BSP, and
  prints data with `std::println`.

The application layer contains no Linux headers and never accesses a file
descriptor, so it can be tested on a development machine with a fake UART.

## Runtime flow

1. Scan supported serial devices under `/dev` and sort them by path.
2. Ask the user for a list index; repeat the prompt for non-numeric or
   out-of-range input.
3. Open the selected device and configure 115200 baud, 8 data bits, no parity,
   1 stop bit, and no flow control.
4. Register the UART descriptor with epoll and send
   `Hello from example_uart(epoll)!`.
5. Block in epoll without busy polling and print `Received: ...` for data.

No discovered port or end-of-input is a clean exit. Open, configuration,
read/write, or epoll errors are reported and return a non-zero status.

## ARM cross-build

The default is `arm-none-linux-gnueabihf`, Release, and static linking:

```powershell
cmake -S . -B build-arm -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf
cmake --build build-arm -j
```

If the toolchain is not in `PATH`, provide its `bin` directory:

```powershell
cmake -S . -B build-arm -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=arm-none-linux-gnueabihf `
  -DTOOLCHAIN_PATH=C:/path/to/arm-none-linux-gnueabihf/bin
```

The executable is `build-arm/src/example_uart`. Verify it with:

```powershell
arm-none-linux-gnueabihf-readelf -h build-arm/src/example_uart
arm-none-linux-gnueabihf-readelf -l build-arm/src/example_uart
```

The ELF header must report 32-bit ARM, and a static program must not have an
`INTERP` program header.

## Local business-logic tests

On Windows, the `local` configuration builds only portable application logic
and fake-UART tests:

```powershell
cmake -S . -B build-local -G "MinGW Makefiles" `
  -DTARGET_PLATFORM=local `
  -DBUILD_TESTING=ON
cmake --build build-local -j
ctest --test-dir build-local --output-on-failure
```

On local Linux, the same configuration also builds a directly runnable
`example_uart`.

## Code style

The project uses C++23 and `std::print/std::println` `{}` formatting without
`{fmt}`. `.clang-format` specifies four-space indentation, 100 columns, Allman
braces for functions, and automatic include sorting.

- Types: `UpperCamelCase`
- Functions: `lowerCamelCase`
- Locals and members: `snake_case`
- Constants: `kUpperCamelCase`
