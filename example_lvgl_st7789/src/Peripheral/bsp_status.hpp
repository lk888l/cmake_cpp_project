#pragma once

#include <cstdint>

namespace bsp {

enum class Status : uint8_t {
    ok = 0,
    invalid_argument,
    invalid_state,
    not_found,
    permission_denied,
    io_error,
};

const char* toString(Status status);

} // namespace bsp

