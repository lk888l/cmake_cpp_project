#pragma once

#include "core/config.hpp"
#include "core/media.hpp"
#include "platform/rockchip/dma_buffer.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace camera_display {

enum class MediaStage : std::uint8_t {
    None,
    Rkaiq,
    System,
    VideoInput,
    Rga,
};

class RockchipMediaPipeline final : public FrameSource,
                                    public HardwareConverter {
public:
    explicit RockchipMediaPipeline(CameraConfig config,
                                   std::uint16_t outputWidth = 240,
                                   std::uint16_t outputHeight = 135);
    ~RockchipMediaPipeline() override;

    RockchipMediaPipeline(const RockchipMediaPipeline&) = delete;
    RockchipMediaPipeline& operator=(const RockchipMediaPipeline&) = delete;

    MediaStatus start() override;
    MediaStatus acquire(CapturedFrame& frame,
                        std::chrono::milliseconds timeout) override;
    void release(CapturedFrame& frame) noexcept override;
    void stop() noexcept override;
    [[nodiscard]] std::string description() const override;

    MediaStatus convert(const CapturedFrame& source,
                        const Rect& sourceCrop,
                        std::size_t outputSlot) override;
    [[nodiscard]] ConvertedFrame output(std::size_t slot) noexcept override;

    [[nodiscard]] MediaStage failedStage() const noexcept
    {
        return failed_stage_.load();
    }
    [[nodiscard]] std::string lastError() const;
    bool beginCpuRead(std::size_t slot);
    bool endCpuRead(std::size_t slot) noexcept;

private:
    struct Impl;

    bool startRkaiq();
    bool startVideoInput();
    bool startRga();
    void stopVideoInput() noexcept;
    void stopRkaiq() noexcept;
    void setError(MediaStage stage, std::string message);

    CameraConfig config_;
    std::uint16_t output_width_;
    std::uint16_t output_height_;
    std::array<DmaBuffer, 2> outputs_;
    std::array<std::uintptr_t, 2> rga_output_handles_{};
    std::unique_ptr<Impl> impl_;
    std::atomic<MediaStage> failed_stage_{MediaStage::None};
    mutable std::mutex error_mutex_;
    std::string last_error_;
};

} // namespace camera_display
