#include <string>
#include <atomic>
#include <stdexcept>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <rndr/window.hpp>

Window::Window(size_t width, size_t height, std::string title) :
    width(width), height(height), title(std::move(title)) {}

Window::~Window() {
    this->DestroyWindow();
}

void Window::CreateWindow(size_t width, size_t height, std::string title) {
    if (this->window != nullptr) {
        return;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    this->window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
    if (this->window == nullptr) {
        throw std::runtime_error("glfw: failed to create window.");
    }

    this->width = width;
    this->height = height;
    this->title = title;
    this->is_initialized = true;
}

void Window::DestroyWindow() {
    if (this->window != nullptr) {
        glfwDestroyWindow(this->window);
        this->window = nullptr;
    }
    this->is_initialized = false;
}

void Window::Run(std::atomic<bool>& shouldRun) {
    if (this->window == nullptr) {
        shouldRun = false;
        return;
    }

    glfwPollEvents();
    if (glfwWindowShouldClose(this->window)) {
        shouldRun = false;
    }
}