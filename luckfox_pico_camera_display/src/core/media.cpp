#include "core/media.hpp"

namespace camera_display {

const char* toString(MediaStatus status) noexcept
{
    switch (status) {
    case MediaStatus::Ok: return "ok";
    case MediaStatus::InvalidArgument: return "invalid argument";
    case MediaStatus::InvalidState: return "invalid state";
    case MediaStatus::NotFound: return "not found";
    case MediaStatus::Busy: return "busy";
    case MediaStatus::Timeout: return "timeout";
    case MediaStatus::Unsupported: return "unsupported";
    case MediaStatus::IoError: return "I/O error";
    }
    return "unknown";
}

} // namespace camera_display
