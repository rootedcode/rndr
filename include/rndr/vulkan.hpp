#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct FrameData {
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkSemaphore swapchain_sempaphore = VK_NULL_HANDLE;
    VkSemaphore render_semaphore = VK_NULL_HANDLE;
    VkFence render_fence = VK_NULL_HANDLE;
};

class VulkanEngine {
public:
    VulkanEngine(GLFWwindow* window);
    ~VulkanEngine();

    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    void Draw_frames();

private:
    GLFWwindow* window = nullptr;
    bool isInitialized = false;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice chosen_gpu = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_image_format{};
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    VkExtent2D swapchainExtent{};
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkQueue present_queue = VK_NULL_HANDLE;
    std::vector<FrameData> frames;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline graphics_pipeline = VK_NULL_HANDLE;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    uint32_t graphics_queue_family = 0;
    uint32_t present_queue_family = 0;
    uint32_t current_frame = 0;

    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
    void destroy_sync_structures();
    void create_render_pass();
    void create_graphics_pipeline();
    void create_framebuffers();
    void destroy_framebuffers();
    
    void create_swapchain(uint32_t width, uint32_t height);
    void recreate_swapchain();
    void destroy_swapchain();

};