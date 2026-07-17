#include "input/button.hpp"
#include "input/button_state_machine.hpp"

#include <fcntl.h>
#include <linux/gpio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <utility>

namespace gpio_button {
namespace {

constexpr std::uint32_t kStopTag = 1;
constexpr std::uint32_t kTimerTag = 2;
constexpr std::uint32_t kButtonTagBase = 100;
constexpr std::uint32_t kKernelEventBufferSize = 64;
constexpr std::size_t kReadBatchSize = 32;

[[noreturn]] void errnoError(const char* message)
{
    throw std::system_error(errno, std::generic_category(), message);
}

MonotonicTime monotonicNow()
{
    return MonotonicTime{
        std::chrono::duration_cast<MonotonicDuration>(
            std::chrono::steady_clock::now().time_since_epoch())};
}

timespec asTimespec(MonotonicTime value)
{
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(value.time_since_epoch());
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
        value.time_since_epoch() - seconds);
    return {
        .tv_sec = static_cast<time_t>(seconds.count()),
        .tv_nsec = static_cast<long>(nanoseconds.count()),
    };
}

bool pressedFromEvent(const ButtonConfig& config, std::uint32_t event_id)
{
    const bool physical_level_high =
        event_id == GPIO_V2_LINE_EVENT_RISING_EDGE;
    return isPressedLevel(config, physical_level_high);
}

void setNonBlockingCloseOnExec(int fd)
{
    const int status_flags = ::fcntl(fd, F_GETFL);
    if (status_flags < 0 ||
        ::fcntl(fd, F_SETFL, status_flags | O_NONBLOCK) < 0 ||
        ::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        errnoError("fcntl(GPIO line fd)");
    }
}

} // namespace

class ButtonManager::Impl {
public:
    explicit Impl(std::vector<ButtonConfig> configs)
        : configs_(std::move(configs))
    {
        if (configs_.empty()) {
            throw std::invalid_argument("at least one button is required");
        }

        std::unordered_set<std::string> ids;
        std::set<std::pair<std::string, unsigned int>> physical_lines;
        for (const auto& config : configs_) {
            if (config.id.empty() || config.chip_path.empty() ||
                !ids.insert(config.id).second) {
                throw std::invalid_argument(
                    "button ids and chip paths must be non-empty; ids must be unique");
            }
            if (!physical_lines.emplace(config.chip_path, config.line_offset).second) {
                throw std::invalid_argument(
                    "two buttons cannot use the same GPIO chip and line");
            }
            machines_.push_back(
                std::make_unique<detail::ButtonStateMachine>(config));
        }
    }

    ~Impl() { stop(); }

    void setCallback(ButtonCallback callback)
    {
        std::lock_guard lock(callback_mutex_);
        callback_ = std::move(callback);
    }

    [[nodiscard]] bool isRunning() const noexcept
    {
        return running_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::optional<std::string> lastError() const
    {
        std::lock_guard lock(error_mutex_);
        return last_error_;
    }

    void start()
    {
        std::lock_guard lock(lifecycle_mutex_);
        if (running_.load(std::memory_order_acquire) || worker_.joinable() ||
            epoll_fd_ >= 0) {
            throw std::logic_error(
                "ButtonManager is already active; call stop before restarting");
        }

        {
            std::lock_guard error_lock(error_mutex_);
            last_error_.reset();
        }

        try {
            createPollFds();
            requestLines();
            armTimer();
            running_.store(true, std::memory_order_release);
            worker_ = std::jthread([this] { run(); });
        } catch (...) {
            running_.store(false, std::memory_order_release);
            releaseResources();
            throw;
        }
    }

    void stop()
    {
        std::unique_lock lock(lifecycle_mutex_);
        const bool was_running =
            running_.exchange(false, std::memory_order_acq_rel);
        if (!was_running && !worker_.joinable() && epoll_fd_ < 0) {
            return;
        }

        if (was_running && stop_fd_ >= 0) {
            const std::uint64_t signal = 1;
            if (::write(stop_fd_, &signal, sizeof(signal)) < 0 &&
                errno != EAGAIN) {
                std::cerr << "GPIO manager wakeup failed: "
                          << std::strerror(errno) << '\n';
            }
        }

        // Manager lifetime belongs to the main thread. This guard prevents a
        // callback-triggered stop from trying to join itself.
        if (worker_.get_id() == std::this_thread::get_id()) {
            return;
        }

        std::jthread worker = std::move(worker_);
        lock.unlock();
        if (worker.joinable()) {
            worker.join();
        }
        lock.lock();
        releaseResources();
    }

private:
    struct Device {
        int chip_fd{-1};
        int line_fd{-1};
    };

    void createPollFds()
    {
        epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ < 0) {
            errnoError("epoll_create1");
        }

        stop_fd_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (stop_fd_ < 0) {
            errnoError("eventfd");
        }

        timer_fd_ =
            ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (timer_fd_ < 0) {
            errnoError("timerfd_create");
        }

        addFd(stop_fd_, kStopTag);
        addFd(timer_fd_, kTimerTag);
    }

    void addFd(int fd, std::uint32_t tag)
    {
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.u32 = tag;
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
            errnoError("epoll_ctl(EPOLL_CTL_ADD)");
        }
    }

    void requestLines()
    {
        devices_.reserve(configs_.size());
        for (std::size_t index = 0; index < configs_.size(); ++index) {
            const auto& config = configs_[index];
            devices_.emplace_back();
            auto& device = devices_.back();

            device.chip_fd =
                ::open(config.chip_path.c_str(), O_RDONLY | O_CLOEXEC);
            if (device.chip_fd < 0) {
                errnoError("open(GPIO chip)");
            }

            gpiochip_info chip_info{};
            if (::ioctl(device.chip_fd, GPIO_GET_CHIPINFO_IOCTL, &chip_info) < 0) {
                errnoError("ioctl(GPIO_GET_CHIPINFO_IOCTL)");
            }
            if (config.line_offset >= chip_info.lines) {
                throw std::invalid_argument(
                    "GPIO line offset exceeds chip line count");
            }

            gpio_v2_line_request request{};
            request.offsets[0] = config.line_offset;
            std::strncpy(request.consumer, "lvgl-st7789-menu",
                         sizeof(request.consumer) - 1);
            request.config.flags = GPIO_V2_LINE_FLAG_INPUT |
                                   GPIO_V2_LINE_FLAG_EDGE_RISING |
                                   GPIO_V2_LINE_FLAG_EDGE_FALLING;
            request.num_lines = 1;
            request.event_buffer_size = kKernelEventBufferSize;

            if (::ioctl(device.chip_fd, GPIO_V2_GET_LINE_IOCTL, &request) < 0) {
                errnoError("ioctl(GPIO_V2_GET_LINE_IOCTL)");
            }
            device.line_fd = request.fd;
            setNonBlockingCloseOnExec(device.line_fd);

            gpio_v2_line_values values{};
            values.mask = 1;
            if (::ioctl(device.line_fd, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) <
                0) {
                errnoError("ioctl(GPIO_V2_LINE_GET_VALUES_IOCTL)");
            }

            const bool physical_level_high = (values.bits & 1U) != 0;
            machines_[index]->setInitialLevel(
                isPressedLevel(config, physical_level_high), monotonicNow());
            addFd(device.line_fd,
                  kButtonTagBase + static_cast<std::uint32_t>(index));
        }
    }

    void run() noexcept
    {
        try {
            while (running_.load(std::memory_order_acquire)) {
                std::array<epoll_event, 16> ready{};
                const int count = ::epoll_wait(
                    epoll_fd_, ready.data(), static_cast<int>(ready.size()), -1);
                if (count < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    errnoError("epoll_wait");
                }

                for (int i = 0; i < count; ++i) {
                    if (ready[static_cast<std::size_t>(i)].data.u32 == kStopTag) {
                        drain(stop_fd_);
                        return;
                    }
                }

                // Kernel-timestamped GPIO edges take precedence at an exact
                // timer boundary so release can preserve long-press semantics.
                for (int i = 0; i < count; ++i) {
                    const auto tag =
                        ready[static_cast<std::size_t>(i)].data.u32;
                    if (tag >= kButtonTagBase) {
                        readEdges(static_cast<std::size_t>(tag - kButtonTagBase));
                    }
                }
                for (int i = 0; i < count; ++i) {
                    if (ready[static_cast<std::size_t>(i)].data.u32 == kTimerTag) {
                        drain(timer_fd_);
                        fireTimers();
                    }
                }
                armTimer();
            }
        } catch (const std::exception& error) {
            recordError(error.what());
        } catch (...) {
            recordError("unknown exception in GPIO event loop");
        }
    }

    void recordError(std::string message) noexcept
    {
        running_.store(false, std::memory_order_release);
        try {
            {
                std::lock_guard lock(error_mutex_);
                last_error_ = message;
            }
            std::cerr << "GPIO event loop stopped: " << message << '\n';
        } catch (...) {
            // The worker must never terminate the process while reporting an
            // already-fatal device error.
        }
    }

    void readEdges(std::size_t index)
    {
        if (index >= devices_.size()) {
            return;
        }

        std::array<gpio_v2_line_event, kReadBatchSize> events{};
        for (;;) {
            const ssize_t bytes =
                ::read(devices_[index].line_fd, events.data(), sizeof(events));
            if (bytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                if (errno == EINTR) {
                    continue;
                }
                errnoError("read(GPIO v2 line events)");
            }
            if (bytes == 0) {
                throw std::runtime_error(
                    "GPIO line event fd closed unexpectedly");
            }
            if (bytes % static_cast<ssize_t>(sizeof(gpio_v2_line_event)) != 0) {
                throw std::runtime_error("short GPIO v2 line event record");
            }

            const auto count = static_cast<std::size_t>(bytes) /
                               sizeof(gpio_v2_line_event);
            for (std::size_t event_index = 0; event_index < count;
                 ++event_index) {
                const auto& event = events[event_index];
                if (event.id != GPIO_V2_LINE_EVENT_RISING_EDGE &&
                    event.id != GPIO_V2_LINE_EVENT_FALLING_EDGE) {
                    continue;
                }
                const auto timestamp = MonotonicTime{MonotonicDuration{
                    static_cast<MonotonicDuration::rep>(event.timestamp_ns)}};
                dispatch(machines_[index]->processEdge(
                    pressedFromEvent(configs_[index], event.id), timestamp));
            }
        }
    }

    void fireTimers()
    {
        const auto current_time = monotonicNow();
        for (auto& state : machines_) {
            dispatch(state->processTimers(current_time));
        }
    }

    void dispatch(const std::vector<ButtonEvent>& events)
    {
        ButtonCallback callback;
        {
            std::lock_guard lock(callback_mutex_);
            callback = callback_;
        }
        if (callback) {
            for (const auto& event : events) {
                callback(event);
            }
        }
    }

    void armTimer()
    {
        std::optional<MonotonicTime> deadline;
        for (const auto& state : machines_) {
            const auto candidate = state->nextDeadline();
            if (candidate && (!deadline || *candidate < *deadline)) {
                deadline = candidate;
            }
        }

        itimerspec timer{};
        if (deadline) {
            timer.it_value = asTimespec(*deadline);
        }
        if (::timerfd_settime(timer_fd_, TFD_TIMER_ABSTIME, &timer, nullptr) <
            0) {
            errnoError("timerfd_settime");
        }
    }

    static void drain(int fd)
    {
        std::uint64_t ignored{};
        (void)::read(fd, &ignored, sizeof(ignored));
    }

    void releaseResources() noexcept
    {
        for (auto& device : devices_) {
            if (device.line_fd >= 0) {
                ::close(device.line_fd);
            }
            if (device.chip_fd >= 0) {
                ::close(device.chip_fd);
            }
        }
        devices_.clear();

        if (timer_fd_ >= 0) {
            ::close(timer_fd_);
            timer_fd_ = -1;
        }
        if (stop_fd_ >= 0) {
            ::close(stop_fd_);
            stop_fd_ = -1;
        }
        if (epoll_fd_ >= 0) {
            ::close(epoll_fd_);
            epoll_fd_ = -1;
        }
    }

    std::vector<ButtonConfig> configs_;
    std::vector<std::unique_ptr<detail::ButtonStateMachine>> machines_;
    std::vector<Device> devices_;
    std::mutex lifecycle_mutex_;
    std::mutex callback_mutex_;
    mutable std::mutex error_mutex_;
    ButtonCallback callback_;
    std::optional<std::string> last_error_;
    std::atomic_bool running_{false};
    std::jthread worker_;
    int epoll_fd_{-1};
    int stop_fd_{-1};
    int timer_fd_{-1};
};

ButtonManager::ButtonManager(std::vector<ButtonConfig> buttons)
    : impl_(std::make_unique<Impl>(std::move(buttons)))
{
}

ButtonManager::~ButtonManager() = default;

void ButtonManager::setCallback(ButtonCallback callback)
{
    impl_->setCallback(std::move(callback));
}

void ButtonManager::start()
{
    impl_->start();
}

void ButtonManager::stop()
{
    impl_->stop();
}

bool ButtonManager::isRunning() const noexcept
{
    return impl_->isRunning();
}

std::optional<std::string> ButtonManager::lastError() const
{
    return impl_->lastError();
}

} // namespace gpio_button
