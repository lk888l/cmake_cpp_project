#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace camera_display {

struct FrameMetadata final {
    std::uint64_t sequence{};
    std::uint64_t capture_timestamp_us{};
    std::chrono::steady_clock::time_point ready_at{};
};

template <std::size_t SlotCount>
class LatestFrameMailbox final {
    static_assert(SlotCount >= 2, "at least two slots are required");

public:
    struct Ticket final {
        std::size_t slot{};
        FrameMetadata metadata{};
    };

    std::optional<std::size_t> acquireForWrite()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_) return std::nullopt;

        if (ready_slot_) {
            const std::size_t slot = *ready_slot_;
            states_[slot] = State::Writing;
            ready_slot_.reset();
            ++overwritten_;
            return slot;
        }
        for (std::size_t index = 0; index < SlotCount; ++index) {
            if (states_[index] == State::Free) {
                states_[index] = State::Writing;
                return index;
            }
        }
        ++rejected_;
        return std::nullopt;
    }

    bool publish(std::size_t slot, FrameMetadata metadata)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slot >= SlotCount || states_[slot] != State::Writing) return false;
        if (stopping_) {
            states_[slot] = State::Free;
            return false;
        }
        if (ready_slot_) {
            states_[*ready_slot_] = State::Free;
            ++overwritten_;
        }
        metadata_[slot] = metadata;
        states_[slot] = State::Ready;
        ready_slot_ = slot;
        condition_.notify_one();
        return true;
    }

    std::optional<Ticket> waitForRead(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait_for(lock, timeout, [this] {
            return stopping_ || ready_slot_.has_value();
        });
        if (!ready_slot_) return std::nullopt;
        const std::size_t slot = *ready_slot_;
        ready_slot_.reset();
        states_[slot] = State::Reading;
        return Ticket{slot, metadata_[slot]};
    }

    bool releaseRead(std::size_t slot)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slot >= SlotCount || states_[slot] != State::Reading) return false;
        states_[slot] = State::Free;
        return true;
    }

    void cancelWrite(std::size_t slot)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (slot < SlotCount && states_[slot] == State::Writing) {
            states_[slot] = State::Free;
        }
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        condition_.notify_all();
    }

    [[nodiscard]] std::uint64_t overwrittenCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return overwritten_;
    }

    [[nodiscard]] std::uint64_t rejectedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return rejected_;
    }

private:
    enum class State : std::uint8_t {
        Free,
        Writing,
        Ready,
        Reading,
    };

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::array<State, SlotCount> states_{};
    std::array<FrameMetadata, SlotCount> metadata_{};
    std::optional<std::size_t> ready_slot_;
    std::uint64_t overwritten_{};
    std::uint64_t rejected_{};
    bool stopping_{};
};

} // namespace camera_display

