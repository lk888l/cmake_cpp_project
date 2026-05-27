#include <iostream>
#include <future>
#include <thread>
#include <queue>
#include <vector>
#include <functional>
#include <condition_variable>
#include <mutex>

class ThreadPool
{
    public:
    ThreadPool(size_t n)
        : stop_(false)
    {
        for (size_t i = 0; i < n; ++i)
        {
            workers_.emplace_back([this] {
                while (true)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock lock(mutex_);

                        cv_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                        });

                        if (stop_ && tasks_.empty())
                            return;

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<
            std::packaged_task<ReturnType()>
        >(
            std::bind(
                std::forward<F>(f),
                std::forward<Args>(args)...
            )
        );

        std::future<ReturnType> future = task->get_future();

        {
            std::lock_guard lock(mutex_);

            tasks_.emplace([task] {
                (*task)();
            });
        }

        cv_.notify_one();

        return future;
    }

    ~ThreadPool()
    {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }

        cv_.notify_all();

        for (auto& t : workers_)
            t.join();
    }

    private:
        std::vector<std::thread> workers_;

        std::queue<std::function<void()>> tasks_;

        std::mutex mutex_;
        std::condition_variable cv_;

        bool stop_;
};


int heavy_task(int x)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return x * x;
}

int main()
{
    ThreadPool pool(4);

    std::vector<std::future<int>> futures;

    for (int i = 0; i < 8; ++i)
    {
        futures.push_back(
            pool.submit(heavy_task, i)
        );
    }

    for (auto& f : futures)
    {
        std::cout << f.get() << std::endl;
    }

    std::cout << " concurrent threads supported: " << std::thread::hardware_concurrency() << std::endl;
}