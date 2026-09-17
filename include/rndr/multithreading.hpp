#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Function, typename... Args>
    auto enqueue(Function&& f, Args&&... args) -> std::future<typename std::invoke_result<Function, Args...>::type> {
        using return_type = typename std::invoke_result<Function, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<Function>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queue_mutex);

            if (stop) {
                throw std::runtime_error("enqueue on stopped thread pool.");
            }

            tasks.emplace([task]() { (*task)(); });
        }
        cv.notify_one();
        return result;
    }

private:
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    bool stop = false;

    std::vector<std::jthread> workers;

    void worker_loop(std::stop_token stop_token);
};
