#pragma once

#include "core/config.hpp"

#include <string>
#include <vector>

namespace camera_display {

struct PreflightResult final {
    std::vector<std::string> errors;
    bool camera_occupied{};
};

[[nodiscard]] PreflightResult preflight(
    const AppConfig& config, bool requireMedia);

} // namespace camera_display
