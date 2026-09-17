#include <rndr/vulkan.hpp>
#include <vulkan/vulkan_core.h>

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <vector>

static inline bool use_validation_layers = true;

namespace {
std::filesystem::path resolve_shader_path(const std::string& shader_name) {
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "shaders" / shader_name,
        std::filesystem::current_path() / "../shaders" / shader_name,
        std::filesystem::current_path() / "build" / "shaders" / shader_name,
        std::filesystem::current_path() / "../build" / "shaders" / shader_name,
        std::filesystem::current_path() / "shaders" / (shader_name + ".spv"),
        std::filesystem::current_path() / "../shaders" / (shader_name + ".spv"),
        std::filesystem::current_path() / "build" / "shaders" / (shader_name + ".spv"),
        std::filesystem::current_path() / "../build" / "shaders" / (shader_name + ".spv"),
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    throw std::runtime_error("Unable to find shader: " + shader_name);
}

std::vector<char> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }

    const std::streamsize file_size = file.tellg();
    if (file_size <= 0) {
        throw std::runtime_error("Shader file is empty: " + path.string());
    }

    std::vector<char> buffer(static_cast<size_t>(file_size));
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), file_size);
    return buffer;
}

VkShaderModule create_shader_module(VkDevice device, const std::string& shader_name) {
    const auto shader_path = resolve_shader_path(shader_name);
    const auto code = read_file(shader_path);
    VkShaderModuleCreateInfo create_info{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                          .codeSize = static_cast<size_t>(code.size()),
                                          .pCode = reinterpret_cast<const uint32_t*>(code.data()) };

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &create_info, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module: " + shader_name);
    }
    return module;
}
}

VulkanEngine::VulkanEngine(GLFWwindow* window) : window(window) {
    this->init_vulkan();
    this->init_swapchain();
    this->frames.resize(MAX_FRAMES_IN_FLIGHT);
    this->init_commands();
    this->init_sync_structures();
    this->create_render_pass();
    this->create_graphics_pipeline();
    this->create_framebuffers();
    isInitialized = true;
};

VulkanEngine::~VulkanEngine() {
    if (this->isInitialized) {
        vkDeviceWaitIdle(this->device);
        this->destroy_framebuffers();
        if (this->graphics_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(this->device, this->graphics_pipeline, nullptr);
            this->graphics_pipeline = VK_NULL_HANDLE;
        }
        if (this->pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(this->device, this->pipeline_layout, nullptr);
            this->pipeline_layout = VK_NULL_HANDLE;
        }
        if (this->render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(this->device, this->render_pass, nullptr);
            this->render_pass = VK_NULL_HANDLE;
        }
        this->destroy_sync_structures();

        for (const auto& frame : this->frames) {
            vkDestroyFence(this->device, frame.render_fence, nullptr);
            vkDestroyCommandPool(this->device, frame.command_pool, nullptr);
        }

        this->destroy_swapchain();
        vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
        vkDestroyDevice(this->device, nullptr);
        vkb::destroy_debug_utils_messenger(this->instance, this->debug_messenger);
        vkDestroyInstance(this->instance, nullptr);
    }
    this->isInitialized = false;
}

void VulkanEngine::init_vulkan() {
    vkb::InstanceBuilder builder;

    auto vkb_build = builder.set_app_name("Example Vulkan Application")
        .request_validation_layers(use_validation_layers)
        .use_default_debug_messenger()
        .require_api_version(1, 4, 0)
        .build();

    vkb::Instance vkb_instance = vkb_build.value();

    instance = vkb_instance.instance;
    debug_messenger = vkb_instance.debug_messenger;

    glfwCreateWindowSurface(instance, this->window, nullptr, &this->surface);

    if (this->surface == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to create window surface.");
    }
    
    VkPhysicalDeviceVulkan14Features features_14{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES };

    VkPhysicalDeviceVulkan13Features features_13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features_13.dynamicRendering = vk::True;
    features_13.synchronization2 = vk::True;

    VkPhysicalDeviceVulkan12Features features_12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features_12.bufferDeviceAddress = vk::True;
    features_12.descriptorIndexing = vk::True;

    vkb::PhysicalDeviceSelector selector{ vkb_instance };
    vkb::PhysicalDevice physical_device = selector
        .set_minimum_version(1, 4)
        .set_required_features_14(features_14)
        .set_required_features_13(features_13)
        .set_required_features_12(features_12)
        .set_surface(this->surface)
        .select()
        .value();

    vkb::DeviceBuilder device_builder{ physical_device };
    vkb::Device vkb_device = device_builder.build().value();

    this->device = vkb_device.device;
    this->chosen_gpu = physical_device.physical_device;

    this->graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    this->graphics_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::init_swapchain() {
    this->create_swapchain(1280, 720);
};

void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder{ this->chosen_gpu, this->device, this->surface };
    
    this->swapchain_image_format = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkb_swapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = this->swapchain_image_format, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

        this->swapchainExtent = vkb_swapchain.extent;
        this->swapchain = vkb_swapchain.swapchain;
        this->swapchain_images = vkb_swapchain.get_images().value();
        this->swapchain_image_views = vkb_swapchain.get_image_views().value();
    
};

void VulkanEngine::destroy_swapchain() {
    for (VkImageView image_view : this->swapchain_image_views) {
        vkDestroyImageView(this->device, image_view, nullptr);
    }

    this->swapchain_image_views.clear();
    this->swapchain_images.clear();

    if (this->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(this->device, this->swapchain, nullptr);
        this->swapchain = VK_NULL_HANDLE;
    }
};

void VulkanEngine::init_commands() {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkCommandPoolCreateInfo command_pool_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, .queueFamilyIndex = this->graphics_queue_family };
        if (vkCreateCommandPool(this->device, &command_pool_info, nullptr, &this->frames[i].command_pool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool.");
        }
        VkCommandBufferAllocateInfo command_buffer_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = this->frames[i].command_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1 };
        if (vkAllocateCommandBuffers(this->device, &command_buffer_info, &this->frames[i].command_buffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffer.");
        }
    }
};

void VulkanEngine::destroy_sync_structures() {
    for (VkSemaphore semaphore : this->image_available_semaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(this->device, semaphore, nullptr);
        }
    }
    for (VkSemaphore semaphore : this->render_finished_semaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(this->device, semaphore, nullptr);
        }
    }
    this->image_available_semaphores.clear();
    this->render_finished_semaphores.clear();
}

void VulkanEngine::init_sync_structures() {
    this->destroy_sync_structures();

    this->image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
    this->render_finished_semaphores.resize(this->swapchain_images.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (this->image_available_semaphores[i] == VK_NULL_HANDLE &&
            vkCreateSemaphore(this->device, &semaphore_info, nullptr, &this->image_available_semaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphores.");
        }
    }

    for (size_t i = 0; i < this->render_finished_semaphores.size(); ++i) {
        if (vkCreateSemaphore(this->device, &semaphore_info, nullptr, &this->render_finished_semaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphores.");
        }
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (this->frames[i].render_fence == VK_NULL_HANDLE) {
            VkFenceCreateInfo fence_info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
            if (vkCreateFence(this->device, &fence_info, nullptr, &this->frames[i].render_fence) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create fence.");
            }
        }
    }
};

void VulkanEngine::recreate_swapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(this->window, &width, &height);

    while (width == 0 || height == 0) {
        glfwWaitEvents();
        glfwGetFramebufferSize(this->window, &width, &height);
    }

    vkDeviceWaitIdle(this->device);
    this->destroy_framebuffers();
    this->destroy_sync_structures();
    this->destroy_swapchain();
    this->create_swapchain(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    this->init_sync_structures();
    this->create_framebuffers();
}

void VulkanEngine::create_render_pass() {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = this->swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_reference{};
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(this->device, &render_pass_info, nullptr, &this->render_pass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
}

void VulkanEngine::create_graphics_pipeline() {
    VkShaderModule vert_module = create_shader_module(this->device, "basic_triangle.vert.spv");
    VkShaderModule frag_module = create_shader_module(this->device, "basic_triangle.frag.spv");

    VkPipelineShaderStageCreateInfo vert_stage{};
    vert_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage.module = vert_module;
    vert_stage.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage{};
    frag_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage.module = frag_module;
    frag_stage.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = { vert_stage, frag_stage };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(this->swapchainExtent.width);
    viewport.height = static_cast<float>(this->swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = this->swapchainExtent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(this->device, &pipeline_layout_info, nullptr, &this->pipeline_layout) != VK_SUCCESS) {
        vkDestroyShaderModule(this->device, frag_module, nullptr);
        vkDestroyShaderModule(this->device, vert_module, nullptr);
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = this->pipeline_layout;
    pipeline_info.renderPass = this->render_pass;
    pipeline_info.subpass = 0;

    if (vkCreateGraphicsPipelines(this->device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->graphics_pipeline) != VK_SUCCESS) {
        vkDestroyPipelineLayout(this->device, this->pipeline_layout, nullptr);
        this->pipeline_layout = VK_NULL_HANDLE;
        vkDestroyShaderModule(this->device, frag_module, nullptr);
        vkDestroyShaderModule(this->device, vert_module, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline.");
    }

    vkDestroyShaderModule(this->device, frag_module, nullptr);
    vkDestroyShaderModule(this->device, vert_module, nullptr);
}

void VulkanEngine::create_framebuffers() {
    this->destroy_framebuffers();

    this->framebuffers.resize(this->swapchain_images.size());
    for (size_t i = 0; i < this->swapchain_images.size(); ++i) {
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = this->render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &this->swapchain_image_views[i];
        framebuffer_info.width = this->swapchainExtent.width;
        framebuffer_info.height = this->swapchainExtent.height;
        framebuffer_info.layers = 1;

        if (vkCreateFramebuffer(this->device, &framebuffer_info, nullptr, &this->framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer.");
        }
    }
}

void VulkanEngine::destroy_framebuffers() {
    for (VkFramebuffer framebuffer : this->framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(this->device, framebuffer, nullptr);
        }
    }
    this->framebuffers.clear();
}

void VulkanEngine::Draw_frames() {
    if (this->window == nullptr || this->device == VK_NULL_HANDLE || this->swapchain == VK_NULL_HANDLE) {
        return;
    }

    const uint32_t frame_index = this->current_frame % MAX_FRAMES_IN_FLIGHT;
    auto& frame = this->frames[frame_index];

    vkWaitForFences(this->device, 1, &frame.render_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(this->device, 1, &frame.render_fence);

    const VkSemaphore image_available_semaphore = this->image_available_semaphores[frame_index];
    uint32_t swapchain_image_index = 0;
    VkResult acquire_result = vkAcquireNextImageKHR(
        this->device,
        this->swapchain,
        UINT64_MAX,
        image_available_semaphore,
        VK_NULL_HANDLE,
        &swapchain_image_index
    );
    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR || acquire_result == VK_SUBOPTIMAL_KHR) {
        this->recreate_swapchain();
        return;
    }
    if (acquire_result != VK_SUCCESS) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    vkResetCommandBuffer(frame.command_buffer, 0);
    VkCommandBufferBeginInfo begin_info{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    if (vkBeginCommandBuffer(frame.command_buffer, &begin_info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer.");
    }

    VkClearValue clear_color{ .color = { {0.0f, 0.2f, 0.8f, 1.0f} } };
    VkRenderPassBeginInfo render_pass_begin_info{ .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                                 .renderPass = this->render_pass,
                                                 .framebuffer = this->framebuffers[swapchain_image_index],
                                                 .renderArea = { { 0, 0 }, this->swapchainExtent },
                                                 .clearValueCount = 1,
                                                 .pClearValues = &clear_color };

    vkCmdBeginRenderPass(frame.command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->graphics_pipeline);
    vkCmdDraw(frame.command_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(frame.command_buffer);

    if (vkEndCommandBuffer(frame.command_buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer.");
    }

    VkSubmitInfo submit_info{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkSemaphore wait_semaphores[] = { image_available_semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;
    VkSemaphore signal_semaphores[] = { this->render_finished_semaphores[swapchain_image_index] };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    if (vkQueueSubmit(this->graphics_queue, 1, &submit_info, frame.render_fence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer.");
    }

    VkPresentInfoKHR present_info{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    VkSwapchainKHR swapchains[] = { this->swapchain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &swapchain_image_index;
    VkResult present_result = vkQueuePresentKHR(this->graphics_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR) {
        this->recreate_swapchain();
        return;
    }
    if (present_result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    this->current_frame = (this->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}