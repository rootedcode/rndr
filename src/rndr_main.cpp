#include <functional>
#include <memory>
#include <print>
#include <thread>

#include <rndr/application.hpp>

int main() {
    constexpr size_t window_width = 1280;
    constexpr size_t window_height = 720;
    constexpr auto window_title = "Hello, GLFW!";

    std::unique_ptr<Application> app = std::make_unique<Application>(window_width, window_height, window_title);

    for (size_t i = 0; i < std::thread::hardware_concurrency(); i++) {
        app->QueueTask([] {
            std::println("Hello, from thread {}!", std::hash<std::thread::id>{}(std::this_thread::get_id()));
        });
    }

    app->Run();

    return 0;
}