#include <functional>
#include <mutex>
#include <queue>

#include <rndr/multithreading.hpp>

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        throw std::invalid_argument("ThreadPool: number of threads must be greater than zero.");
    }
    workers.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this](std::stop_token stop_token) {
            worker_loop(stop_token);
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        stop = true;
    }
    cv.notify_all();
}

void ThreadPool::worker_loop(std::stop_token stop_token) {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            cv.wait(lock, [this, stop_token] {
                return stop || stop_token.stop_requested() || !tasks.empty();
            });

            const bool stop_requested = stop || stop_token.stop_requested();
            if (stop_requested && tasks.empty()) {
                return;
            }

            task = std::move(tasks.front());
            tasks.pop();
        }

        task();
    }
} 