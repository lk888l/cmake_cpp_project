#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "bsp/uart/uart_port.hpp"

namespace app {

using ReceiveHandler = std::function<void(std::string_view)>;

struct UartDemoConfig {
    std::string greeting = "Hello from example_uart(epoll)!\n";
};

class UartDemoApp {
public:
    UartDemoApp(bsp::UartPort& uart,
                ReceiveHandler receive_handler,
                UartDemoConfig config = {});

    void start();
    void processNext();

private:
    bsp::UartPort& uart_;
    ReceiveHandler receive_handler_;
    UartDemoConfig config_;
    bool started_ = false;
};

} // namespace app
