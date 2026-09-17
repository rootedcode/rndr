#pragma once

#include <atomic>
#include <cstddef>
#include <string>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

class Window {
public:
    Window(size_t width, size_t height, std::string title);
    ~Window();

    void CreateWindow(size_t width, size_t height, std::string title);
    void DestroyWindow();
    void Run(std::atomic<bool>& shouldRun);

    GLFWwindow* GetHandle() const { return window; }

private:
    GLFWwindow* window = nullptr;
    size_t width = 0;
    size_t height = 0;
    std::string title;
    bool is_initialized = false;
};