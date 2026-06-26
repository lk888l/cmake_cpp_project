#pragma once

#include <atomic>
#include <stop_token>
#include <thread>

#include "app_task.hpp"

/// @brief Linux platform runner — executes an AppTask on a std::jthread.
///
/// Owns the thread; the task is passed by reference and must outlive the runner.
///
/// Usage:
/// @code
///   MyTask task("worker");          // AppTask subclass
///   LinuxTaskRunner runner(task);   // bind platform
///   runner.start();
///   runner.stop();
/// @endcode
class LinuxTaskRunner
{
public:
    explicit LinuxTaskRunner(AppTask& task);
    ~LinuxTaskRunner();

    bool start();
    void stop();

    bool isRunning() const
    {
        return running_.load(std::memory_order_acquire);
    }

private:
    void runnerEntry(std::stop_token stoken);

    AppTask& task_;
    std::jthread thread_;
    std::atomic<bool> running_{false};
};
