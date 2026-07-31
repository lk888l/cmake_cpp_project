#pragma once

#include "core/io.hpp"

#include <string>

namespace camera_display {

class LinuxGpioOutput final : public OutputPin {
public:
    LinuxGpioOutput(std::string chipPath, unsigned int lineOffset,
                    std::string consumer);
    ~LinuxGpioOutput() override;

    IoStatus request(bool initialHigh) override;
    IoStatus set(bool high) override;
    void release() noexcept override;
    [[nodiscard]] bool isRequested() const noexcept override { return line_fd_ >= 0; }

private:
    std::string chip_path_;
    unsigned int line_offset_;
    std::string consumer_;
    int line_fd_{-1};
};

} // namespace camera_display
