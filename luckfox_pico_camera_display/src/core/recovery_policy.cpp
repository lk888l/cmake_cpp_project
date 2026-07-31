#include "core/recovery_policy.hpp"

#include <array>
#include <cstddef>

namespace camera_display {

RecoveryPolicy::RecoveryPolicy(std::uint32_t maximumRestarts) noexcept
    : maximum_restarts_(maximumRestarts)
{
}

bool RecoveryPolicy::beginRecovery() noexcept
{
    if (state_ == PipelineState::Stopping || state_ == PipelineState::Failed
        || attempts_ >= maximum_restarts_) {
        state_ = PipelineState::Failed;
        return false;
    }
    ++attempts_;
    state_ = PipelineState::Recovering;
    return true;
}

void RecoveryPolicy::markRunning() noexcept
{
    state_ = PipelineState::Running;
}

std::chrono::milliseconds RecoveryPolicy::backoff() const noexcept
{
    constexpr std::array<std::uint32_t, 3> backoffs{{250, 1000, 2000}};
    if (attempts_ == 0) return std::chrono::milliseconds{0};
    const std::size_t index = static_cast<std::size_t>(attempts_ - 1U);
    return std::chrono::milliseconds{
        backoffs[index < backoffs.size() ? index : backoffs.size() - 1U]};
}

const char* toString(PipelineState state) noexcept
{
    switch (state) {
    case PipelineState::Starting: return "starting";
    case PipelineState::Running: return "running";
    case PipelineState::Recovering: return "recovering";
    case PipelineState::Failed: return "failed";
    case PipelineState::Stopping: return "stopping";
    }
    return "unknown";
}

} // namespace camera_display

