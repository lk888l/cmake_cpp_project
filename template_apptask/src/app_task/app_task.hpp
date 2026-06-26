#pragma once

#include <atomic>
#include <string>

/// @brief Platform-independent task interface.
///
/// Business logic inherits this class and implements main().
/// A platform runner (e.g. LinuxTaskRunner, FreeRTOSTaskRunner) drives it.
///
/// Lifecycle (managed by the runner):
///   1. runner.start()  → calls main()
///   2. main() loops,   polls shouldExit() to cooperate
///   3. runner.stop()   → sets exit flag, joins context
///   4. cleanup()       → called after main() returns
///
/// Usage:
/// @code
///   class MyTask : public AppTask {
///       void main() override {
///           while (!shouldExit()) { /* work */ }
///       }
///   };
///
///   MyTask task("worker");
///   LinuxTaskRunner runner(task);
///   runner.start();
///   // ...
///   runner.stop();
/// @endcode
class AppTask
{
public:
    explicit AppTask(std::string name = {}) : name_(std::move(name)) {}
    virtual ~AppTask() = default;

    /// Human-readable name for logging / debug.
    const std::string& name() const { return name_; }

    // ---- hooks for derived task logic ----

    /// Task body.  Subclasses implement their loop here.
    /// Poll shouldExit() each iteration to cooperate with stop().
    virtual void main() = 0;

    /// Called once after main() returns (even if by exception).
    /// Override to release resources.
    virtual void cleanup() {}

    /// Cooperative exit check — call this regularly in main().
    bool shouldExit() const
    {
        return exit_requested_.load(std::memory_order_relaxed);
    }

    // ---- for runner use only ----
    void requestExit()
    {
        exit_requested_.store(true, std::memory_order_relaxed);
    }

    void resetExitFlag()
    {
        exit_requested_.store(false, std::memory_order_relaxed);
    }

private:
    std::string name_;
    std::atomic<bool> exit_requested_{false};
};
