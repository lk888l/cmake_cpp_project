#include "core/io.hpp"

namespace camera_display {

const char* toString(IoStatus status) noexcept
{
    switch (status) {
    case IoStatus::Ok: return "ok";
    case IoStatus::InvalidArgument: return "invalid argument";
    case IoStatus::InvalidState: return "invalid state";
    case IoStatus::NotFound: return "not found";
    case IoStatus::PermissionDenied: return "permission denied";
    case IoStatus::Busy: return "busy";
    case IoStatus::Interrupted: return "interrupted";
    case IoStatus::Timeout: return "timeout";
    case IoStatus::IoError: return "I/O error";
    }
    return "unknown";
}

} // namespace camera_display
