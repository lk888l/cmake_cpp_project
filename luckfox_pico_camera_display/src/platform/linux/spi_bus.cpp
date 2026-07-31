#include "platform/linux/spi_bus.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

namespace camera_display {
namespace {

IoStatus statusFromErrno(int value) noexcept
{
    if (value == ENOENT || value == ENODEV) return IoStatus::NotFound;
    if (value == EACCES || value == EPERM) return IoStatus::PermissionDenied;
    if (value == EBUSY) return IoStatus::Busy;
    if (value == EINTR) return IoStatus::Interrupted;
    return IoStatus::IoError;
}

} // namespace

LinuxSpiBus::LinuxSpiBus(std::string device, std::uint32_t speedHz,
                         std::size_t maximumTransferBytes)
    : device_(std::move(device)),
      speed_hz_(speedHz),
      maximum_transfer_bytes_(maximumTransferBytes)
{
}

LinuxSpiBus::~LinuxSpiBus()
{
    close();
}

IoStatus LinuxSpiBus::open()
{
    if (isOpen()) return IoStatus::InvalidState;
    if (device_.empty() || speed_hz_ == 0 || maximum_transfer_bytes_ == 0
        || maximum_transfer_bytes_ > static_cast<std::size_t>(UINT32_MAX)) {
        return IoStatus::InvalidArgument;
    }
    do {
        fd_ = ::open(device_.c_str(), O_RDWR | O_CLOEXEC);
    } while (fd_ < 0 && errno == EINTR);
    if (fd_ < 0) return statusFromErrno(errno);

    std::uint8_t mode{};
    std::uint8_t bits{8};
    const auto configure = [this](unsigned long request, const void* value) {
        int result{};
        do {
            result = ::ioctl(fd_, request, value);
        } while (result < 0 && errno == EINTR);
        return result;
    };
    if (configure(SPI_IOC_WR_MODE, &mode) < 0
        || configure(SPI_IOC_WR_BITS_PER_WORD, &bits) < 0
        || configure(SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz_) < 0) {
        const IoStatus status = statusFromErrno(errno);
        close();
        return status;
    }
    return IoStatus::Ok;
}

IoStatus LinuxSpiBus::write(const std::uint8_t* bytes, std::size_t size)
{
    if (!isOpen()) return IoStatus::InvalidState;
    if (bytes == nullptr && size != 0) return IoStatus::InvalidArgument;

    std::size_t offset{};
    while (offset < size) {
        const std::size_t chunk = std::min(maximum_transfer_bytes_, size - offset);
        spi_ioc_transfer transfer{};
        transfer.tx_buf =
            static_cast<decltype(transfer.tx_buf)>(
                reinterpret_cast<std::uintptr_t>(bytes + offset));
        transfer.len = static_cast<decltype(transfer.len)>(chunk);
        transfer.speed_hz = speed_hz_;
        transfer.bits_per_word = 8;

        int result{};
        do {
            result = ::ioctl(fd_, SPI_IOC_MESSAGE(1), &transfer);
            if (result < 0 && errno == EINTR) ++statistics_.interrupted_retries;
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            ++statistics_.errors;
            return statusFromErrno(errno);
        }
        if (static_cast<std::size_t>(result) != chunk) {
            ++statistics_.errors;
            return IoStatus::IoError;
        }
        ++statistics_.transactions;
        statistics_.bytes += chunk;
        offset += chunk;
    }
    return IoStatus::Ok;
}

void LinuxSpiBus::close() noexcept
{
    if (fd_ < 0) return;
    // On Linux the descriptor is released even if close reports EINTR.
    // Retrying can accidentally close a descriptor reused by another thread.
    (void)::close(fd_);
    fd_ = -1;
}

} // namespace camera_display
