#include <rndr/application.hpp>

void Application::initialize_window_and_vulkan(size_t window_width, size_t window_height, const std::string& window_title) {
    if (!glfwInit()) {
        throw std::runtime_error("glfw: failed to initialize program.");
    }

    this->window.CreateWindow(window_width, window_height, window_title);
    if (!this->window.GetHandle()) {
        throw std::runtime_error("glfw: failed to create window.");
    }

    this->vk.emplace(this->window.GetHandle());
}

Application::Application(size_t window_width, size_t window_height, std::string window_title) : 
    window(window_width, window_height, window_title),
    threads(std::make_optional<ThreadPool>(std::thread::hardware_concurrency())) {
    initialize_window_and_vulkan(window_width, window_height, window_title);
}

Application::~Application() {
    shouldRun = false;
    threads.reset();
    vk.reset();
    window.DestroyWindow();
    glfwTerminate();
}

void Application::Run() {
    shouldRun = true;
    while (shouldRun.load()) {
        window.Run(shouldRun);
        if (!shouldRun.load()) {
            break;
        }

        if (vk) {
            vk->Draw_frames();
        }
    }
}