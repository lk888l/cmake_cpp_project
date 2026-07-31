#pragma once

#include <chrono>
#include <cstdint>

namespace camera_display {

enum class PipelineState : std::uint8_t {
    Starting,
    Running,
    Recovering,
    Failed,
    Stopping,
};

class RecoveryPolicy final {
public:
    explicit RecoveryPolicy(std::uint32_t maximumRestarts) noexcept;

    [[nodiscard]] bool beginRecovery() noexcept;
    void markRunning() noexcept;
    void markFailed() noexcept { state_ = PipelineState::Failed; }
    void markStopping() noexcept { state_ = PipelineState::Stopping; }

    [[nodiscard]] PipelineState state() const noexcept { return state_; }
    [[nodiscard]] std::uint32_t attempts() const noexcept { return attempts_; }
    [[nodiscard]] std::chrono::milliseconds backoff() const noexcept;

private:
    std::uint32_t maximum_restarts_{};
    std::uint32_t attempts_{};
    PipelineState state_{PipelineState::Starting};
};

[[nodiscard]] const char* toString(PipelineState state) noexcept;

} // namespace camera_display

