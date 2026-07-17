#include "app/uart_demo_app.hpp"

#include <stdexcept>
#include <utility>

namespace app {

UartDemoApp::UartDemoApp(bsp::UartPort& uart,
                         ReceiveHandler receive_handler,
                         UartDemoConfig config)
    : uart_(uart)
    , receive_handler_(std::move(receive_handler))
    , config_(std::move(config))
{
    if (!receive_handler_) {
        throw std::invalid_argument("UART receive handler must not be empty");
    }
}

void UartDemoApp::start()
{
    if (started_) {
        return;
    }

    uart_.write(config_.greeting);
    started_ = true;
}

void UartDemoApp::processNext()
{
    if (!started_) {
        throw std::logic_error("UART demo must be started before processing data");
    }

    const std::string data = uart_.waitAndRead();
    if (!data.empty()) {
        receive_handler_(data);
    }
}

} // namespace app
