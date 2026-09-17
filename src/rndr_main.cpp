#include <print>
#include <thread>

#include <rndr/application.hpp>

int main() {
    constexpr bool multithreaded = true;
    constexpr size_t num_of_threads = 4;
    constexpr size_t window_width = 1280;
    constexpr size_t window_height = 720;
    constexpr auto window_title = "Hello, GLFW!";

    std::unique_ptr<Application> app;

    if (multithreaded) {
        app = std::make_unique<Application>(window_width, window_height, window_title, num_of_threads);
        for (size_t i = 0; i < num_of_threads; i++) {
            app->QueueTask([] {
                std::println("Hello, from thread {}!", std::this_thread::get_id());
            });
        }
    } else {
        app = std::make_unique<Application>(window_width, window_height, window_title);
        std::println("Hello, from a non-multithreaded application!");
    }

    app->Run();

    return 0;
}