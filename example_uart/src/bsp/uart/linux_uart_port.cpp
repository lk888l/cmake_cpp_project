#include "bsp/uart/linux_uart_port.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <filesystem>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <sys/epoll.h>
#include <termios.h>
#include <unistd.h>

namespace bsp {
namespace {

constexpr std::array<std::string_view, 5> kSerialPortPrefixes = {
    "ttyS", "ttyUSB", "ttyACM", "ttyAMA", "ttyO"};

[[nodiscard]] speed_t toPosixBaudRate(std::uint32_t baud_rate)
{
    switch (baud_rate) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    default:
        throw std::invalid_argument("unsupported UART baud rate: " +
                                    std::to_string(baud_rate));
    }
}

[[nodiscard]] bool hasSerialPrefix(std::string_view name)
{
    return std::ranges::any_of(kSerialPortPrefixes, [name](std::string_view prefix) {
        return name.starts_with(prefix);
    });
}

} // namespace

LinuxUartPort::LinuxUartPort(UartConfig config) : read_buffer_size_(config.read_buffer_size)
{
    if (config.device.empty()) {
        throw std::invalid_argument("UART device path must not be empty");
    }
    if (read_buffer_size_ == 0) {
        throw std::invalid_argument("UART read buffer size must be greater than zero");
    }

    uart_fd_ = ::open(config.device.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (uart_fd_ < 0) {
        throw std::system_error(errno,
                                std::generic_category(),
                                "failed to open UART device " + config.device);
    }

    try {
        configure(config.baud_rate);

        epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ < 0) {
            throw std::system_error(
                errno, std::generic_category(), "failed to create UART epoll instance");
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        event.data.fd = uart_fd_;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, uart_fd_, &event) < 0) {
            throw std::system_error(
                errno, std::generic_category(), "failed to register UART with epoll");
        }
    } catch (...) {
        closeResources();
        throw;
    }
}

LinuxUartPort::~LinuxUartPort()
{
    closeResources();
}

void LinuxUartPort::write(std::string_view data)
{
    std::size_t bytes_written = 0;
    while (bytes_written < data.size()) {
        const ssize_t result =
            ::write(uart_fd_, data.data() + bytes_written, data.size() - bytes_written);
        if (result > 0) {
            bytes_written += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0) {
            throw std::system_error(errno, std::generic_category(), "failed to write UART data");
        }
        throw std::system_error(EIO, std::generic_category(), "UART write made no progress");
    }
}

std::string LinuxUartPort::waitAndRead()
{
    while (true) {
        epoll_event event{};
        const int ready_count = ::epoll_wait(epoll_fd_, &event, 1, -1);
        if (ready_count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::system_error(errno, std::generic_category(), "UART epoll wait failed");
        }

        if (ready_count == 0 || event.data.fd != uart_fd_) {
            continue;
        }

        std::string data(read_buffer_size_, '\0');
        const ssize_t bytes_read = ::read(uart_fd_, data.data(), data.size());
        if (bytes_read > 0) {
            data.resize(static_cast<std::size_t>(bytes_read));
            return data;
        }
        if (bytes_read < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }
        if (bytes_read < 0) {
            throw std::system_error(errno, std::generic_category(), "failed to read UART data");
        }

        throw std::system_error(EIO, std::generic_category(), "UART device was closed");
    }
}

void LinuxUartPort::configure(std::uint32_t baud_rate)
{
    termios options{};
    if (::tcgetattr(uart_fd_, &options) != 0) {
        throw std::system_error(errno, std::generic_category(), "failed to read UART settings");
    }

    const speed_t speed = toPosixBaudRate(baud_rate);
    if (::cfsetispeed(&options, speed) != 0 || ::cfsetospeed(&options, speed) != 0) {
        throw std::system_error(errno, std::generic_category(), "failed to set UART baud rate");
    }

    options.c_cflag &= static_cast<tcflag_t>(~(PARENB | CSTOPB | CSIZE | CRTSCTS));
    options.c_cflag |= CS8 | CREAD | CLOCAL;
    options.c_iflag &= static_cast<tcflag_t>(~(IXON | IXOFF | IXANY));
    options.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ECHOE | ISIG));
    options.c_oflag &= static_cast<tcflag_t>(~OPOST);
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    if (::tcsetattr(uart_fd_, TCSANOW, &options) != 0) {
        throw std::system_error(errno, std::generic_category(), "failed to apply UART settings");
    }
}

void LinuxUartPort::closeResources() noexcept
{
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
    if (uart_fd_ >= 0) {
        ::close(uart_fd_);
        uart_fd_ = -1;
    }
}

std::vector<std::string> discoverSerialPorts()
{
    std::vector<std::string> ports;
    std::error_code iterator_error;
    std::filesystem::directory_iterator iterator("/dev", iterator_error);
    const std::filesystem::directory_iterator end;
    if (iterator_error) {
        throw std::system_error(iterator_error, "failed to scan /dev for UART devices");
    }

    while (iterator != end) {
        const auto& entry = *iterator;
        std::error_code type_error;
        const bool is_character_device = entry.is_character_file(type_error);
        const std::string name = entry.path().filename().string();
        if (!type_error && is_character_device && hasSerialPrefix(name)) {
            ports.push_back(entry.path().string());
        }

        iterator.increment(iterator_error);
        if (iterator_error) {
            throw std::system_error(iterator_error, "failed while scanning /dev");
        }
    }

    std::ranges::sort(ports);
    return ports;
}

} // namespace bsp
