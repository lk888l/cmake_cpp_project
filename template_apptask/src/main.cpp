#include "app_task.hpp"
#include "app_task_linux.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

// ============================================================================
// 示例：自定义任务 — 只继承 AppTask，不依赖任何平台
// ============================================================================
class BlinkTask : public AppTask
{
public:
    using AppTask::AppTask;  // 继承构造函数（传入 name）

private:
    void main() override
    {
        int count = 0;
        while (!shouldExit())          // 协作式退出检查
        {
            std::cout << "[" << name() << "] tick " << ++count << '\n';
            std::this_thread::sleep_for(500ms);
        }
        std::cout << "[" << name() << "] exiting main()\n";
    }

    void cleanup() override            // main() 返回后自动调用
    {
        std::cout << "[" << name() << "] cleanup() called\n";
    }
};

// ============================================================================
int main()
{
    std::cout << "=== AppTask demo ===\n\n";

    // 1. 创建业务任务（纯 AppTask，平台无关）
    BlinkTask task("blinker");

    // 2. 用 Linux 平台 runner 驱动它
    LinuxTaskRunner runner(task);

    // 3. 启动
    runner.start();
    std::cout << "runner started: " << runner.isRunning() << '\n';

    // 4. 主线程等 2 秒
    std::this_thread::sleep_for(2s);

    // 5. 停止
    runner.stop();
    std::cout << "runner stopped: " << runner.isRunning() << '\n';

    std::cout << "\n=== done ===\n";
}
