#pragma once

#include <iosfwd>

namespace camera_display {

enum class ProbeExit : int {
    Success = 0,
    MissingRequiredCapability = 20,
};

ProbeExit runProbe(std::ostream& output);

} // namespace camera_display
