#include <charconv>
#include <cstdio>
#include <iostream>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include "app/uart_demo_app.hpp"
#include "bsp/uart/linux_uart_port.hpp"

namespace {

constexpr std::uint32_t kDefaultBaudRate = 115200;
constexpr std::size_t kDefaultReadBufferSize = 256;

[[nodiscard]] std::optional<std::size_t> selectSerialPort(const std::vector<std::string>& ports)
{
    while (true) {
        std::println("请输入要打开的串口编号 (1-{}):", ports.size());

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::println("输入结束，程序退出。");
            return std::nullopt;
        }

        std::size_t selection = 0;
        const auto [end, error] = std::from_chars(line.data(), line.data() + line.size(), selection);
        if (error != std::errc{} || end != line.data() + line.size()) {
            std::println("无效输入，请输入数字。");
            continue;
        }

        if (selection >= 1 && selection <= ports.size()) {
            return selection - 1;
        }
        std::println("请选择 1 到 {} 之间的编号。", ports.size());
    }
}

} // namespace

int main()
{
    std::println("begin uart example");

    try {
        const std::vector<std::string> ports = bsp::discoverSerialPorts();
        if (ports.empty()) {
            std::println("未发现串口设备，程序退出。");
            return 0;
        }

        std::println("检测到 {} 个串口设备:", ports.size());
        for (std::size_t index = 0; index < ports.size(); ++index) {
            std::println("  [{}] {}", index + 1, ports[index]);
        }

        const std::optional<std::size_t> selected_index = selectSerialPort(ports);
        if (!selected_index) {
            return 0;
        }

        const std::string& selected_port = ports[*selected_index];
        std::println("正在打开串口：{}", selected_port);

        bsp::LinuxUartPort uart(
            {selected_port, kDefaultBaudRate, kDefaultReadBufferSize});
        app::UartDemoApp demo(uart, [](std::string_view data) {
            std::println("Received: {}", data);
        });

        demo.start();
        std::println("Event loop started. Waiting for data... (0% CPU usage)");
        while (true) {
            demo.processNext();
        }
    } catch (const std::exception& exception) {
        std::println(stderr, "UART error: {}", exception.what());
        return 1;
    }
}
