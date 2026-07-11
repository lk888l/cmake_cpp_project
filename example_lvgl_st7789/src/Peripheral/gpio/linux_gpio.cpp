#include "gpio/linux_gpio.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/gpio.h>
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

LinuxGpioOutput::LinuxGpioOutput(std::string chip_path,
                                 unsigned int line_offset,
                                 std::string consumer)
    : chip_path_(std::move(chip_path)),
      line_offset_(line_offset),
      consumer_(std::move(consumer))
{
}

LinuxGpioOutput::~LinuxGpioOutput()
{
    deinit();
}

Status LinuxGpioOutput::init(bool initial_high)
{
    if (isInitialized()) return Status::invalid_state;

    const int chip_fd = ::open(chip_path_.c_str(), O_RDONLY | O_CLOEXEC);
    if (chip_fd < 0) return errnoStatus();

    gpiohandle_request request{};
    request.lineoffsets[0] = line_offset_;
    request.flags = GPIOHANDLE_REQUEST_OUTPUT;
    request.default_values[0] = initial_high ? 1U : 0U;
    request.lines = 1;
    std::strncpy(request.consumer_label, consumer_.c_str(), sizeof(request.consumer_label) - 1);

    if (::ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &request) < 0) {
        const Status status = errnoStatus();
        ::close(chip_fd);
        return status;
    }
    ::close(chip_fd);
    line_fd_ = request.fd;
    return Status::ok;
}

Status LinuxGpioOutput::set(bool high)
{
    if (!isInitialized()) return Status::invalid_state;
    gpiohandle_data data{};
    data.values[0] = high ? 1U : 0U;
    return ::ioctl(line_fd_, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) == 0
        ? Status::ok : errnoStatus();
}

void LinuxGpioOutput::deinit()
{
    if (line_fd_ >= 0) {
        ::close(line_fd_);
        line_fd_ = -1;
    }
}

} // namespace bsp
