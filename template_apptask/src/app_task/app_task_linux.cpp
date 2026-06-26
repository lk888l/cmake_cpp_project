#include "app_task_linux.hpp"

#include <utility>

// ============================================================================
// LinuxTaskRunner
// ============================================================================

LinuxTaskRunner::LinuxTaskRunner(AppTask& task)
    : task_(task)
{
}

LinuxTaskRunner::~LinuxTaskRunner()
{
    stop();
}

bool LinuxTaskRunner::start()
{
    if (isRunning())
        return false;

    task_.resetExitFlag();

    thread_ = std::jthread([this](std::stop_token stoken) {
        runnerEntry(stoken);
    });

    return true;
}

void LinuxTaskRunner::stop()
{
    if (!isRunning())
        return;

    task_.requestExit();
    thread_.request_stop();

    if (thread_.joinable())
        thread_.join();
}

void LinuxTaskRunner::runnerEntry(std::stop_token /*stoken*/)
{
    running_.store(true, std::memory_order_release);

    try
    {
        task_.main();
    }
    catch (...)
    {
        task_.cleanup();
        running_.store(false, std::memory_order_release);
        throw;
    }

    task_.cleanup();
    running_.store(false, std::memory_order_release);
}
