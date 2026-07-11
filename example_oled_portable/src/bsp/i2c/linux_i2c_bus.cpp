#include "linux_i2c_bus.hpp"

#include <cerrno>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <memory>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace bsp {
namespace {

I2CStatus fromErrno(int error)
{
    switch (error) {
    case EINVAL: return I2CStatus::invalid_argument;
    case ENOENT:
    case ENODEV:
    case ENXIO: return I2CStatus::not_found;
    case ETIMEDOUT: return I2CStatus::timeout;
    default: return I2CStatus::io_error;
    }
}

class LinuxI2CDevice final : public I2CDevice {
public:
    LinuxI2CDevice(std::string device_path, std::uint8_t address)
        : address_(address), fd_(::open(device_path.c_str(), O_RDWR)) {}

    ~LinuxI2CDevice() override { if (fd_ >= 0) { ::close(fd_); } }
    bool isOpen() const { return fd_ >= 0; }

    I2CStatus write(const std::uint8_t* data, std::size_t length) override
    {
        if (fd_ < 0 || data == nullptr || length == 0 || length > 0xFFFFU) {
            return I2CStatus::invalid_argument;
        }
        i2c_msg message{};
        message.addr = address_;
        message.len = static_cast<__u16>(length);
        message.buf = const_cast<__u8*>(data);
        i2c_rdwr_ioctl_data transfer{&message, 1};
        return ::ioctl(fd_, I2C_RDWR, &transfer) < 0 ? fromErrno(errno) : I2CStatus::ok;
    }

    I2CStatus writeRead(const std::uint8_t* write_data, std::size_t write_length,
                        std::uint8_t* read_data, std::size_t read_length) override
    {
        if (fd_ < 0 || write_data == nullptr || write_length == 0 || read_data == nullptr ||
            read_length == 0 || write_length > 0xFFFFU || read_length > 0xFFFFU) {
            return I2CStatus::invalid_argument;
        }
        i2c_msg messages[2]{};
        messages[0] = {address_, 0, static_cast<__u16>(write_length),
                       const_cast<__u8*>(write_data)};
        messages[1] = {address_, I2C_M_RD, static_cast<__u16>(read_length), read_data};
        i2c_rdwr_ioctl_data transfer{messages, 2};
        return ::ioctl(fd_, I2C_RDWR, &transfer) < 0 ? fromErrno(errno) : I2CStatus::ok;
    }

private:
    std::uint8_t address_;
    int fd_ = -1;
};

} // namespace

LinuxI2CBus::LinuxI2CBus(LinuxI2CBusConfig config) : config_(std::move(config)) {}
LinuxI2CBus::~LinuxI2CBus() { deinit(); }

I2CStatus LinuxI2CBus::init()
{
    if (config_.device_path.empty()) { return I2CStatus::invalid_argument; }
    initialized_ = true;
    return I2CStatus::ok;
}

I2CStatus LinuxI2CBus::deinit()
{
    initialized_ = false;
    return I2CStatus::ok;
}

I2CDeviceResult LinuxI2CBus::createDevice(std::uint8_t address, std::uint32_t)
{
    if (!initialized_) { return {I2CStatus::invalid_state, nullptr}; }
    if (address > 0x7FU) { return {I2CStatus::invalid_argument, nullptr}; }
    auto device = std::make_unique<LinuxI2CDevice>(config_.device_path, address);
    if (!device->isOpen()) { return {fromErrno(errno), nullptr}; }
    return {I2CStatus::ok, std::move(device)};
}

} // namespace bsp

