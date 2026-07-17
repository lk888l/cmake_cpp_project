#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace bsp {

struct UartConfig {
    std::string device;
    std::uint32_t baud_rate = 115200;
    std::size_t read_buffer_size = 256;
};

class UartPort {
public:
    virtual ~UartPort() = default;

    virtual void write(std::string_view data) = 0;
    [[nodiscard]] virtual std::string waitAndRead() = 0;
};

} // namespace bsp
