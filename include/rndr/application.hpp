#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include <rndr/multithreading.hpp>
#include <rndr/vulkan.hpp>
#include <rndr/window.hpp>

class Application {
public:
    Application(size_t window_width, size_t window_height, std::string window_title);

    Application(size_t window_width, size_t window_height, std::string window_title, size_t num_of_threads);

    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Initialize();
    void Run();

    template <typename Function, typename... Args>
    void QueueTask(Function&& function, Args&&... args) {
        if (threads) {
            threads->enqueue(std::forward<Function>(function), std::forward<Args>(args)...);
        }
    }

private:
    void initialize_window_and_vulkan(size_t window_width, size_t window_height, const std::string& window_title);

    Window window;
    std::optional<VulkanEngine> vk;
    std::optional<ThreadPool> threads;
    std::atomic<bool> shouldRun{false};
};
