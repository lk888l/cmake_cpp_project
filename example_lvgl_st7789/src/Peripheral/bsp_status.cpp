#include "bsp_status.hpp"

namespace bsp {

const char* toString(Status status)
{
    switch (status) {
    case Status::ok: return "ok";
    case Status::invalid_argument: return "invalid_argument";
    case Status::invalid_state: return "invalid_state";
    case Status::not_found: return "not_found";
    case Status::permission_denied: return "permission_denied";
    case Status::io_error: return "io_error";
    default: return "unknown";
    }
}

} // namespace bsp

