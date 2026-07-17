#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/uart_demo_app.hpp"

namespace {

class FakeUartPort final : public bsp::UartPort {
public:
    void write(std::string_view data) override
    {
        if (fail_write) {
            throw std::runtime_error("fake UART write failure");
        }
        writes.emplace_back(data);
    }

    std::string waitAndRead() override
    {
        if (fail_read) {
            throw std::runtime_error("fake UART read failure");
        }
        if (reads.empty()) {
            return {};
        }

        std::string data = std::move(reads.front());
        reads.pop_front();
        return data;
    }

    bool fail_write = false;
    bool fail_read = false;
    std::vector<std::string> writes;
    std::deque<std::string> reads;
};

template <typename Exception, typename Function>
[[nodiscard]] bool throws(Function&& function)
{
    try {
        std::forward<Function>(function)();
    } catch (const Exception&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void expect(bool condition, std::string_view message, int& failures)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void testGreetingIsWrittenOnce(int& failures)
{
    FakeUartPort uart;
    app::UartDemoApp demo(uart, [](std::string_view) {});

    demo.start();
    demo.start();

    expect(uart.writes.size() == 1, "start writes the greeting exactly once", failures);
    expect(!uart.writes.empty() && uart.writes.front() == "Hello from example_uart(epoll)!\n",
           "start writes the configured default greeting",
           failures);
}

void testReceivedDataIsForwarded(int& failures)
{
    FakeUartPort uart;
    uart.reads.emplace_back("first payload");
    uart.reads.emplace_back("");

    std::vector<std::string> received;
    app::UartDemoApp demo(uart, [&received](std::string_view data) {
        received.emplace_back(data);
    });

    demo.start();
    demo.processNext();
    demo.processNext();

    expect(received.size() == 1, "empty UART reads are not forwarded", failures);
    expect(!received.empty() && received.front() == "first payload",
           "received UART data reaches the callback unchanged",
           failures);
}

void testInvalidUsageAndErrorsPropagate(int& failures)
{
    FakeUartPort not_started_uart;
    app::UartDemoApp not_started_demo(not_started_uart, [](std::string_view) {});
    expect(throws<std::logic_error>([&not_started_demo] { not_started_demo.processNext(); }),
           "processing before start is rejected",
           failures);

    FakeUartPort write_failure_uart;
    write_failure_uart.fail_write = true;
    app::UartDemoApp write_failure_demo(write_failure_uart, [](std::string_view) {});
    expect(throws<std::runtime_error>([&write_failure_demo] { write_failure_demo.start(); }),
           "UART write failures propagate",
           failures);

    FakeUartPort read_failure_uart;
    read_failure_uart.fail_read = true;
    app::UartDemoApp read_failure_demo(read_failure_uart, [](std::string_view) {});
    read_failure_demo.start();
    expect(throws<std::runtime_error>([&read_failure_demo] { read_failure_demo.processNext(); }),
           "UART read failures propagate",
           failures);

    FakeUartPort missing_handler_uart;
    expect(throws<std::invalid_argument>([&missing_handler_uart] {
               app::UartDemoApp demo(missing_handler_uart, {});
           }),
           "an empty receive callback is rejected",
           failures);
}

} // namespace

int main()
{
    int failures = 0;
    testGreetingIsWrittenOnce(failures);
    testReceivedDataIsForwarded(failures);
    testInvalidUsageAndErrorsPropagate(failures);

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All UART application tests passed\n";
    return 0;
}
