#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "bsp/uart/uart_port.hpp"

namespace bsp {

class LinuxUartPort final : public UartPort {
public:
    explicit LinuxUartPort(UartConfig config);
    ~LinuxUartPort() override;

    LinuxUartPort(const LinuxUartPort&) = delete;
    LinuxUartPort& operator=(const LinuxUartPort&) = delete;
    LinuxUartPort(LinuxUartPort&&) = delete;
    LinuxUartPort& operator=(LinuxUartPort&&) = delete;

    void write(std::string_view data) override;
    [[nodiscard]] std::string waitAndRead() override;

private:
    void configure(std::uint32_t baud_rate);
    void closeResources() noexcept;

    int uart_fd_ = -1;
    int epoll_fd_ = -1;
    std::size_t read_buffer_size_ = 0;
};

[[nodiscard]] std::vector<std::string> discoverSerialPorts();

} // namespace bsp
