/**
 * @file main.cpp
 * @brief UART 示例程序入口
 * @details 演示如何使用 Uart 类进行串口通信
 * @version 1.0.0
 * @date 2026-05-01
 */

#include <print>
#include <filesystem>
#include <algorithm>
#include <array>
#include <string>
#include <iostream>
#include "Peripheral/uart/uart.hpp"
#include "Peripheral/Epoll/EpollManager.hpp"

static std::vector<std::string> list_serial_ports() {
    std::vector<std::string> ports;
    const std::array<std::string_view, 5> prefixes = {"ttyS", "ttyUSB", "ttyACM", "ttyAMA", "ttyO"};

    for (auto const& entry : std::filesystem::directory_iterator("/dev")) {
        if (!entry.is_character_file()) continue;
        auto name = entry.path().filename().string();
        if (std::any_of(prefixes.begin(), prefixes.end(), [&](auto prefix) {
                return name.rfind(prefix, 0) == 0;
            })) {
            ports.push_back(entry.path().string());
        }
    }

    // std::sort(ports.begin(), ports.end());
    return ports;
}

int main() {
    std::println("begin uart example");

    try {
        auto ports = list_serial_ports();
        if (ports.empty()) {
            std::println("未发现串口设备，程序退出。");
            return 0;
        }
        std::println("检测到 {} 个串口设备:", ports.size());
        for (size_t i = 0; i < ports.size(); ++i) {
            std::println("  [{}] {}", i + 1, ports[i]);
        }

        int selection = 0;
        while (true) {
            std::println("请输入要打开的串口编号 (1-{}): ", ports.size());
            std::string line;
            if (!std::getline(std::cin, line)) {
                std::println("输入结束，程序退出。");
                return 0;
            }

            try {
                selection = std::stoi(line);
            } catch (...) {
                std::println("无效输入，请输入数字。");
                continue;
            }

            if (selection >= 1 && selection <= static_cast<int>(ports.size())) {
                break;
            }
            std::println("请选择 1 到 {} 之间的编号。", ports.size());
        }

        const auto selected_port = ports[selection - 1];
        std::println("正在打开串口：{}", selected_port);

        Uart uart(selected_port, 115200);
        EpollManager epoll;
        epoll.add_watch(uart.get_fd(), EPOLLIN);

        std::println("Event Loop started. Waiting for data... (0% CPU usage)");
        uart.write("Hello from example_uart(epoll)!\n");

        auto response = uart.read();
        if (!response.empty()) {
            std::println("Received: {}", response);
        }

        while (true) {
            auto ready_fds = epoll.wait(-1);
            for (int fd : ready_fds) {
                if (fd == uart.get_fd()) {
                    auto data = uart.read();
                    if (!data.empty()) {
                        std::println("Received: {}", data);
                    }
                }
            }
        }

    } catch (const std::exception& ex) {
        std::println("Uart error: {}", ex.what());
    }

    return 0;
}