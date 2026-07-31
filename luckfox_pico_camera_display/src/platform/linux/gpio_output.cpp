#include "platform/linux/gpio_output.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/gpio.h>
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

LinuxGpioOutput::LinuxGpioOutput(std::string chipPath,
                                 unsigned int lineOffset,
                                 std::string consumer)
    : chip_path_(std::move(chipPath)),
      line_offset_(lineOffset),
      consumer_(std::move(consumer))
{
}

LinuxGpioOutput::~LinuxGpioOutput()
{
    release();
}

IoStatus LinuxGpioOutput::request(bool initialHigh)
{
    if (isRequested()) return IoStatus::InvalidState;
    if (chip_path_.empty() || consumer_.empty()) return IoStatus::InvalidArgument;

    int chipFd{};
    do {
        chipFd = ::open(chip_path_.c_str(), O_RDONLY | O_CLOEXEC);
    } while (chipFd < 0 && errno == EINTR);
    if (chipFd < 0) return statusFromErrno(errno);

    gpiohandle_request lineRequest{};
    lineRequest.lineoffsets[0] = line_offset_;
    lineRequest.flags = GPIOHANDLE_REQUEST_OUTPUT;
    lineRequest.default_values[0] = initialHigh ? 1U : 0U;
    lineRequest.lines = 1;
    std::strncpy(lineRequest.consumer_label, consumer_.c_str(),
                 sizeof(lineRequest.consumer_label) - 1);

    int result{};
    do {
        result = ::ioctl(chipFd, GPIO_GET_LINEHANDLE_IOCTL, &lineRequest);
    } while (result < 0 && errno == EINTR);
    const int savedErrno = errno;
    (void)::close(chipFd);
    if (result < 0) return statusFromErrno(savedErrno);
    line_fd_ = lineRequest.fd;
    return IoStatus::Ok;
}

IoStatus LinuxGpioOutput::set(bool high)
{
    if (!isRequested()) return IoStatus::InvalidState;
    gpiohandle_data data{};
    data.values[0] = high ? 1U : 0U;
    int result{};
    do {
        result = ::ioctl(line_fd_, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
    } while (result < 0 && errno == EINTR);
    return result == 0 ? IoStatus::Ok : statusFromErrno(errno);
}

void LinuxGpioOutput::release() noexcept
{
    if (line_fd_ < 0) return;
    (void)::close(line_fd_);
    line_fd_ = -1;
}

} // namespace camera_display
