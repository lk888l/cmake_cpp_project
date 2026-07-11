#include "spi/linux_spi_bus.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace bsp {
namespace {

Status errnoStatus()
{
    if (errno == ENOENT || errno == ENODEV) return Status::not_found;
    if (errno == EACCES || errno == EPERM) return Status::permission_denied;
    return Status::io_error;
}

} // namespace

LinuxSpiBus::LinuxSpiBus(LinuxSpiConfig config) : config_(std::move(config)) {}
LinuxSpiBus::~LinuxSpiBus() { deinit(); }

Status LinuxSpiBus::init()
{
    if (isInitialized()) return Status::invalid_state;
    if (config_.speed_hz == 0 || config_.bits_per_word == 0 || config_.max_transfer_bytes == 0) {
        return Status::invalid_argument;
    }

    fd_ = ::open(config_.device.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) return errnoStatus();

    if (::ioctl(fd_, SPI_IOC_WR_MODE, &config_.mode) < 0 ||
        ::ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &config_.bits_per_word) < 0 ||
        ::ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &config_.speed_hz) < 0) {
        const Status status = errnoStatus();
        deinit();
        return status;
    }
    return Status::ok;
}

Status LinuxSpiBus::write(const uint8_t* data, size_t length)
{
    if (!isInitialized()) return Status::invalid_state;
    if (data == nullptr && length != 0) return Status::invalid_argument;

    size_t offset = 0;
    while (offset < length) {
        const size_t chunk = std::min(config_.max_transfer_bytes, length - offset);
        spi_ioc_transfer transfer{};
        transfer.tx_buf = reinterpret_cast<unsigned long>(data + offset);
        transfer.len = static_cast<decltype(transfer.len)>(chunk);
        transfer.speed_hz = config_.speed_hz;
        transfer.bits_per_word = config_.bits_per_word;
        if (::ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer) < 0) return errnoStatus();
        offset += chunk;
    }
    return Status::ok;
}

void LinuxSpiBus::deinit()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

} // namespace bsp
